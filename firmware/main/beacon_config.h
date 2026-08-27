#pragma once

#include <stdint.h>

/* ---- WiFi beacon ---- */
#define SSID_PREFIX               "S3-BEACON"
/* SSID max 32 chars, the "-XXXXXX" suffix takes 7. Too long a prefix would
 * silently truncate the suffix, making every device share one SSID. */
_Static_assert(sizeof(SSID_PREFIX) - 1 + 7 <= 32,
               "SSID_PREFIX too long: the MAC suffix would be truncated, and every device in the batch would share one SSID");
/* "" -> open network; non-empty -> WPA2-PSK. This AP is beacon-only, so a
 * password guards nothing. */
#define AP_PASSWORD               ""
/* 63 = WPA2-PSK limit and the size of wifi_ap_config_t.password; 8 = WPA2
 * minimum. Checked here so a bad value fails the build instead of being
 * silently truncated at runtime. */
_Static_assert(sizeof(AP_PASSWORD) - 1 <= 63,
               "AP_PASSWORD exceeds the WPA2-PSK limit of 63 characters");
_Static_assert(sizeof(AP_PASSWORD) == 1 || sizeof(AP_PASSWORD) - 1 >= 8,
               "AP_PASSWORD must be at least 8 characters when non-empty, or empty for an open network");

#define AP_CHANNEL                1
_Static_assert(AP_CHANNEL >= 1 && AP_CHANNEL <= 14,
               "AP_CHANNEL must be 1..14");

/* Units of TU (1 TU = 1024us). 100 TU is ~102.4ms */
#define WIFI_BEACON_INTERVAL_TU   100
/* Out-of-range makes esp_wifi_set_config return INVALID_ARG, which aborts
 * under ESP_ERROR_CHECK and boot-loops the device. */
_Static_assert(WIFI_BEACON_INTERVAL_TU >= 100 && WIFI_BEACON_INTERVAL_TU <= 60000
               && WIFI_BEACON_INTERVAL_TU % 100 == 0,
               "WIFI_BEACON_INTERVAL_TU must be 100..60000 and a multiple of 100");

/* ---- BLE iBeacon ---- */
/* 4E211BD8-3977-4319-A93A-CDB7921A9D77, this project's own UUID.
 * Swap in another deployment's UUID to interoperate with it. */
#define IBEACON_UUID { \
    0x4E, 0x21, 0x1B, 0xD8, 0x39, 0x77, 0x43, 0x19, \
    0xA9, 0x3A, 0xCD, 0xB7, 0x92, 0x1A, 0x9D, 0x77 }

/* Reference RSSI at 1m */
#define IBEACON_TX_POWER          ((int8_t)-59)
/* Advertising interval (ms). A coexistence choice, not a protocol floor. */
#define BLE_ADV_INTERVAL_MS       100
/* Below 20ms ble_gap_adv_validate rejects the interval: no advertising, and
 * the device still looks healthy. */
_Static_assert(BLE_ADV_INTERVAL_MS >= 20,
               "BLE_ADV_INTERVAL_MS is below the 20ms protocol floor");

/* ---- Status LED ---- */
/* Board D3: GPIO1 -> R3 1K -> LED -> GND, so a high level lights it. */
#define LED_GPIO                  1
/* Toggle interval, so 500 gives a 1Hz blink. */
#define LED_TOGGLE_MS             500

/* ---- Runtime ---- */
#define HEARTBEAT_INTERVAL_MS     30000
