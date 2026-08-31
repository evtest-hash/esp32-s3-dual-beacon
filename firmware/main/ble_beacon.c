#include "ble_beacon.h"
#include "beacon_config.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

static const char *TAG = "ble_beacon";

/* Static storage required: NimBLE reads this after ble_beacon_start() returns. */
static uint8_t s_adv_data[IBEACON_ADV_LEN];

/* Set by ble_hs_id_infer_auto() in on_sync(). */
static uint8_t s_own_addr_type;

/* Reported by ble_beacon_is_advertising() in the heartbeat log. */
static bool s_adv_running = false;

/* esp_power_level_t is a 3 dBm ladder: level 0 is -24 dBm, level 8 is 0 dBm,
 * level 14 is +18 dBm, and +20 dBm is bolted on at level 15. */
static int level_to_dbm(esp_power_level_t level)
{
    if (level == ESP_PWR_LVL_INVALID) {
        return 0;
    }
    if (level == ESP_PWR_LVL_P20) {     /* also catches the deprecated P21 alias */
        return 20;
    }
    return ((int)level - 8) * 3;
}

/* Runs after nimble_port_init(), which is what enables the controller: setting
 * the power before that has nothing to set it on.
 *
 * Both the requested and the actual level are logged rather than just the
 * request, because the request is not trustworthy on its own. The ESP-IDF
 * headers contradict each other about the default (one comment says P3, the
 * other P9), and the PHY init data can cap the ceiling below what is asked
 * for. The read-back is the only statement about this chip worth believing --
 * and it is what a measured RSSI delta should be compared against. */
static void set_tx_power(void)
{
    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, BLE_TX_POWER_LEVEL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_ble_tx_power_set failed: %s - advertising at the controller default",
                 esp_err_to_name(err));
    }

    esp_power_level_t actual = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    if (actual == ESP_PWR_LVL_INVALID) {
        ESP_LOGW(TAG, "adv tx power: controller reports an invalid level");
        return;
    }

    ESP_LOGI(TAG, "adv tx power: requested %d dBm, controller reports %d dBm",
             level_to_dbm(BLE_TX_POWER_LEVEL), level_to_dbm(actual));
}

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = { 0 };

    /* Both NON = ADV_NONCONN_IND. Nothing can connect, so no connected
     * state ever competes with WiFi for radio time. */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_NON;

    /* BLE_HCI_ADV_ITVL_NONCONN_MIN (160) looks like a floor but is a BT 4.x
     * leftover the validator ignores; the real minimum is 32 units. */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS);

    int rc = ble_gap_adv_set_data(s_adv_data, (int)sizeof(s_adv_data));
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_data failed: rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: rc=%d", rc);
        return;
    }

    s_adv_running = true;

    ESP_LOGI(TAG, "iBeacon advertising: itvl=%d ms (%u units), nonconnectable",
             BLE_ADV_INTERVAL_MS, (unsigned)adv_params.itvl_min);
}

/* Advertising can only start once host and controller have synced. Runs again
 * after a host reset, so the flag is cleared here: every failure path below
 * leaves it false, and only start_advertising() sets it. */
static void on_sync(void)
{
    s_adv_running = false;

    int rc = ble_hs_util_ensure_addr(0);   /* 0 = prefer a public address */
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: rc=%d", rc);
        return;
    }

    /* ensure_addr can fall back to a random address and still return 0, so
     * infer_auto() is needed for the actual type. A hardcoded
     * BLE_OWN_ADDR_PUBLIC would fail with ENOADDR and never advertise. */
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: rc=%d", rc);
        return;
    }

    start_advertising();
}

static void on_reset(int reason)
{
    /* Advertising stopped with the reset; on_sync() runs again afterward. */
    s_adv_running = false;
    ESP_LOGW(TAG, "NimBLE host reset, reason=%d", reason);
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();              /* blocks until nimble_port_stop() */
    nimble_port_freertos_deinit();
}

bool ble_beacon_is_advertising(void)
{
    return s_adv_running;
}

void ble_beacon_start(const device_id_t *id)
{
    static const uint8_t uuid[16] = IBEACON_UUID;

    ibeacon_build_payload(uuid, id->major, id->minor,
                          IBEACON_TX_POWER, s_adv_data);

    /* Not ESP_ERROR_CHECK: this allocates for the BT controller after WiFi
     * has taken its memory, so NO_MEM here is deterministic and aborting
     * would reboot forever, killing the working WiFi beacon too. */
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s - BLE beacon disabled, WiFi beacon continues running",
                 esp_err_to_name(err));
        return;
    }

    set_tx_power();

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "NimBLE host started, major=%u minor=%u",
             (unsigned)id->major, (unsigned)id->minor);
}
