#include "beacon_data.h"

#include <stdio.h>
#include <string.h>

/* No ESP-IDF headers here: the host tests compile this file standalone. */

void device_id_derive(const uint8_t mac[6], const char *ssid_prefix, device_id_t *out)
{
    /* snprintf guarantees a NUL is written even on truncation */
    snprintf(out->ssid, sizeof(out->ssid), "%s-%02X%02X%02X",
             ssid_prefix, mac[3], mac[4], mac[5]);

    out->major = mac[3];
    out->minor = ((uint16_t)mac[4] << 8) | (uint16_t)mac[5];
}

void ibeacon_build_payload(const uint8_t uuid[16], uint16_t major, uint16_t minor,
                           int8_t tx_power, uint8_t out[IBEACON_ADV_LEN])
{
    /* Flags 0x06 rather than the strict 0x04 for non-discoverable, matching
     * iBeacon practice -- some scanners expect it. */
    static const uint8_t header[9] = {
        0x02, 0x01, 0x06,
        0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15
    };

    memcpy(out, header, sizeof(header));   /* [0..8]  */
    memcpy(out + 9, uuid, 16);             /* [9..24] */

    out[25] = (uint8_t)(major >> 8);
    out[26] = (uint8_t)(major & 0xFF);
    out[27] = (uint8_t)(minor >> 8);
    out[28] = (uint8_t)(minor & 0xFF);
    out[29] = (uint8_t)tx_power;
}
