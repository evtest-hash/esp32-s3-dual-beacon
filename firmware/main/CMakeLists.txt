idf_component_register(
    SRCS "main.c" "beacon_data.c" "device_id.c" "wifi_beacon.c" "ble_beacon.c" "status_led.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash bt esp_driver_gpio esp_timer
)
