#include "beacon_config.h"
#include "device_id.h"
#include "wifi_beacon.h"
#include "ble_beacon.h"
#include "status_led.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void)
{
    /* First: the WiFi driver reads RF calibration data from NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Started before the radios: the blink then also covers a boot loop, which
     * shows up as a repeatedly interrupted pattern. */
    status_led_start();

    device_id_t id;
    ESP_ERROR_CHECK(device_id_init(&id));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_beacon_start(&id);

    /* WiFi before BLE, the conventional order for the coexistence layer. */
    ble_beacon_start(&id);

    ESP_LOGI(TAG, "dual beacon running");

    /* min-ever always falls during startup; a continued decline in steady
     * state means a leak. adv=NO means BLE never started. */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        ESP_LOGI(TAG, "alive | adv=%s | free heap=%u B | min ever=%u B",
                 ble_beacon_is_advertising() ? "yes" : "NO",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
    }
}
