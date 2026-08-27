#include "device_id.h"
#include "beacon_config.h"

#include "esp_mac.h"
#include "esp_log.h"

static const char *TAG = "device_id";

esp_err_t device_id_init(device_id_t *out)
{
    uint8_t mac[6];

    /* softAP MAC (base+1), not base or STA: it is what both the SSID suffix
     * and the iBeacon Major/Minor derive from, so they stay consistent. */
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac failed: %s", esp_err_to_name(err));
        return err;
    }

    device_id_derive(mac, SSID_PREFIX, out);

    ESP_LOGI(TAG, "mac=%02X:%02X:%02X:%02X:%02X:%02X ssid=\"%s\" major=%u minor=%u",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             out->ssid, (unsigned)out->major, (unsigned)out->minor);

    return ESP_OK;
}
