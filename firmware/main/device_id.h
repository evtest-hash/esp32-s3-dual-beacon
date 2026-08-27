#pragma once

#include "beacon_data.h"
#include "esp_err.h"

/* Reads the softAP MAC, derives the identity, and logs it for field checks.
 * On failure out is undefined. */
esp_err_t device_id_init(device_id_t *out);
