#include "ble_beacon.h"
#include "beacon_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

static const char *TAG = "ble_beacon";

/* Static storage required: NimBLE reads these after ble_beacon_start() returns. */
static uint8_t s_adv_data[IBEACON_ADV_LEN];

/* The scan response carries the device name. It has to be a second packet:
 * the advertisement has 1 byte left of its 31, and a name needs at least 3. */
static uint8_t s_scan_rsp[SCAN_RSP_MAX_LEN];
static size_t  s_scan_rsp_len;

/* Set by ble_hs_id_infer_auto() in on_sync(). */
static uint8_t s_own_addr_type;

/* Reported by ble_beacon_is_advertising() in the heartbeat log. */
static bool s_adv_running = false;

/* On this chip esp_power_level_t is a 3 dBm ladder of 16 levels: level 0 is
 * -24 dBm, level 8 is 0 dBm, level 14 is +18 dBm, and +20 dBm is bolted on at
 * level 15. That range is ESP32-S3's, not every ESP32's -- the original ESP32
 * has 8 levels spanning -12 to +9 dBm, and quoting its numbers here is a
 * common way to arrive at the wrong ceiling. */
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

/* Called from on_sync(), immediately before advertising starts -- NOT from
 * ble_beacon_start().
 *
 * Setting the power straight after nimble_port_init() looks sufficient, since
 * that call is what brings the controller up, but measurement says otherwise:
 * the advertising instance created later comes up at the controller default of
 * +9 dBm and ignores a P20 set before it existed. A/B on a MuseLab dongle, board
 * and receiver both fixed in place: -41 dBm median (n=67) with the power set
 * early, -29 dBm (n=79) with it set here. That 12 dB step is the +9 -> +20 dBm
 * the two paths command, and a sweep of the whole ladder on the same board put
 * -41 dBm exactly at the +9 dBm rung.
 *
 * on_sync() also runs again after a host reset, so this re-applies the level
 * every time advertising is rebuilt rather than once at boot.
 *
 * The read-back is logged but is NOT proof. esp_ble_tx_power_get() reports the
 * level the last advertising instance came up at, so before ble_gap_adv_start()
 * it returns the PREVIOUS value, not the one just set. That is exactly how the
 * early-set bug stayed invisible for three releases: it logged "controller
 * reports 20 dBm" while advertising at 9. Only a measured RSSI delta settles
 * this. */
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

    /* conn_mode NON still means nothing can connect, so no connected state ever
     * competes with WiFi for radio time. disc_mode GEN is what moves the PDU
     * from ADV_NONCONN_IND to ADV_SCAN_IND -- see ble_gap_adv_type() in
     * NimBLE, which picks the type off disc_mode once conn_mode is NON.
     *
     * The cost is real: the radio now opens a receive window after each
     * advertisement in case a scanner sends SCAN_REQ, and answers it. This is
     * no longer a transmit-only device. What it buys is a name in the scan
     * response, without which the beacon shows up in every generic scanner as
     * an anonymous MAC.
     *
     * Passive scanners, which is what iBeacon consumers are, never send
     * SCAN_REQ and see the same advertisement as before, byte for byte.
     *
     * It also settles an inconsistency: the Flags byte has always said
     * General Discoverable while disc_mode said otherwise. */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* BLE_HCI_ADV_ITVL_NONCONN_MIN (160) looks like a floor but is a BT 4.x
     * leftover the validator ignores; the real minimum is 32 units. */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS);

    int rc = ble_gap_adv_set_data(s_adv_data, (int)sizeof(s_adv_data));
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_data failed: rc=%d", rc);
        return;
    }

    /* Not fatal: losing the name costs identification in a scanner list, not
     * the beacon itself, so carry on advertising without it. */
    rc = ble_gap_adv_rsp_set_data(s_scan_rsp, (int)s_scan_rsp_len);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_adv_rsp_set_data failed: rc=%d - advertising without a name", rc);
    }

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: rc=%d", rc);
        return;
    }

    s_adv_running = true;

    ESP_LOGI(TAG, "iBeacon advertising: itvl=%d ms (%u units), scannable, nonconnectable",
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

    /* Before start_advertising(), not after: the level is latched into the
     * advertising instance when that instance is created. */
    set_tx_power();

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

    /* The name is the SSID, which device_id_derive() built from this chip's own
     * softAP MAC. Nothing here is a literal: every board names itself, and the
     * two radios answer to the same string. */
    s_scan_rsp_len = scan_rsp_build_name(id->ssid, s_scan_rsp);

    /* Not ESP_ERROR_CHECK: this allocates for the BT controller after WiFi
     * has taken its memory, so NO_MEM here is deterministic and aborting
     * would reboot forever, killing the working WiFi beacon too. */
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s - BLE beacon disabled, WiFi beacon continues running",
                 esp_err_to_name(err));
        return;
    }

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "NimBLE host started, major=%u minor=%u",
             (unsigned)id->major, (unsigned)id->minor);
}
