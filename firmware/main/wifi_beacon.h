#pragma once

#include "beacon_data.h"

/* Broadcasts the SSID only: no services, DHCP disabled. Aborts on failure.
 * Requires nvs_flash_init(), esp_netif_init() and esp_event_loop_create_default(). */
void wifi_beacon_start(const device_id_t *id);
