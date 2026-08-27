#include "wifi_beacon.h"
#include "beacon_config.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "wifi_beacon";

/* Registered after esp_netif_create_default_wifi_ap() on purpose: esp_event
 * runs handlers in registration order, and that call installs the default
 * AP_START handler which starts the DHCP server -- so this one must come
 * second to stop it. Per-event rather than once before esp_wifi_start()
 * because esp_netif_stop() resets dhcps_status to INIT on AP_STOP, so a
 * one-shot stop would not survive an AP restart. */
static void on_ap_start(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    (void)base;
    (void)event_id;
    (void)data;

    esp_netif_t *ap_netif = (esp_netif_t *)arg;
    esp_err_t err = esp_netif_dhcps_stop(ap_netif);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "DHCP server stopped (beacon-only AP)");
    } else if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGI(TAG, "DHCP server was not running");
    } else {
        ESP_LOGW(TAG, "dhcps_stop failed: %s", esp_err_to_name(err));
    }
}

void wifi_beacon_start(const device_id_t *id)
{
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_AP_START, on_ap_start, ap_netif, NULL));

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t cfg = { 0 };
    size_t ssid_len = strlen(id->ssid);
    if (ssid_len > sizeof(cfg.ap.ssid)) {   /* unreachable, id->ssid is char[33] */
        ssid_len = sizeof(cfg.ap.ssid);
    }
    memcpy(cfg.ap.ssid, id->ssid, ssid_len);
    cfg.ap.ssid_len        = (uint8_t)ssid_len;
    cfg.ap.channel         = AP_CHANNEL;
    cfg.ap.ssid_hidden     = 0;               /* must be scannable */
    cfg.ap.max_connection  = 1;               /* smallest legal value, cannot be 0 */
    cfg.ap.beacon_interval = WIFI_BEACON_INTERVAL_TU;

    if (AP_PASSWORD[0] == '\0') {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
        memcpy(cfg.ap.password, AP_PASSWORD, strlen(AP_PASSWORD));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "softAP up: ssid=\"%s\" ch=%d auth=%s beacon=%dTU",
             id->ssid, AP_CHANNEL,
             AP_PASSWORD[0] ? "WPA2-PSK" : "OPEN",
             WIFI_BEACON_INTERVAL_TU);
}
