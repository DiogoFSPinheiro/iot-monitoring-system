#pragma once

#include <stdbool.h>
#include <BH1750.h>

/**
 * @brief Initialise the BH1750FVI light sensor.
 *
 * Configures continuous high-resolution mode (1 lux resolution, 120 ms
 * measurement time).  Wire.begin() must have been called before this.
 *
 * Call once from setup() before the FreeRTOS scheduler starts.
 * The caller must hold i2c_mutex while calling this function, or call it
 * before the scheduler starts (no contention).
 *
 * @return true on success, false if the sensor does not ACK.
 */
bool bh1750_init();

/**
 * @brief Read light intensity from the BH1750FVI.
 *
 * The caller MUST acquire i2c_mutex before calling this function and
 * release it immediately afterwards.
 *
 * @param[out] out  Light intensity in lux (0–65535).
 * @return true on success, false on a read error.
 */
bool bh1750_read(float *out);
