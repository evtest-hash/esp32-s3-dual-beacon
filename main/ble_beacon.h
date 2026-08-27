#pragma once

#include <stdbool.h>

#include "beacon_data.h"

/* Starts non-connectable iBeacon advertising (ADV_NONCONN_IND). Brings up the
 * NimBLE host task and returns; advertising begins later from the sync
 * callback. Requires nvs_flash_init(). */
void ble_beacon_start(const device_id_t *id);

/* For the heartbeat log: advertising can fail while the device otherwise
 * looks healthy, so this needs to be visible. */
bool ble_beacon_is_advertising(void);
