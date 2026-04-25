#pragma once

#include <stdbool.h>

#include <avr/io.h>

#define BH1750_ADDR     0x23
#define BH1750_POWER_ON 0x01
#define BH1750_CONT_HR  0x10  // Continuous High Resolution Mode, 1 lux, 120ms

// Loop-count timeout for TWINT polling. FreeRTOS owns Timer1 in CTC mode,
// so TCNT1-based timing would false-trigger on every tick reset.
// At 16MHz each TWI byte takes ~1440 cycles; 10000 iterations gives ~27x margin.
#define TWI_TIMEOUT_ITERS 10000U

/**
 * @brief Initialise the BH1750FVI light sensor.
 *
 * Configures continuous high-resolution mode (1 lux resolution, 120 ms
 * measurement time). Uses direct TWI register access — no Wire library needed.
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
