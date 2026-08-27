#pragma once

#include <stdint.h>

/* SSID: max 32 chars + NUL */
#define DEVICE_ID_SSID_MAX 33

typedef struct {
    char     ssid[DEVICE_ID_SSID_MAX];
    uint16_t major;
    uint16_t minor;
} device_id_t;

/* Pure function. mac and out must be non-NULL.
 *   ssid  = ssid_prefix + "-" + uppercase hex of mac[3..5]
 *   major = mac[3]
 *   minor = (mac[4] << 8) | mac[5]  */
void device_id_derive(const uint8_t mac[6], const char *ssid_prefix, device_id_t *out);

/* iBeacon packet size, of the 31-byte AD space */
#define IBEACON_ADV_LEN 30

/* Pure function. Fills out[0..29]:
 *   [0..2]   02 01 06            Flags
 *   [3..8]   1A FF 4C 00 02 15   len / MSD / Apple / iBeacon type / len
 *   [9..24]  UUID
 *   [25..26] Major, big-endian
 *   [27..28] Minor, big-endian
 *   [29]     Measured Power  */
void ibeacon_build_payload(const uint8_t uuid[16], uint16_t major, uint16_t minor,
                           int8_t tx_power, uint8_t out[IBEACON_ADV_LEN]);
