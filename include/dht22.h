#pragma once

#include <stdbool.h>

/**
 * @brief Initialise the DHT22 sensor.
 *
 * Call once from setup() before the FreeRTOS scheduler starts.
 *
 * @return true always (library does not report init failure).
 */
bool dht22_init();

/**
 * @brief Read temperature from the DHT22.
 *
 * The DHT22 datasheet mandates at least 2 s between reads.
 * The caller (task_dht22) is responsible for enforcing this delay.
 *
 * @param[out] out  Temperature in degrees Celsius.
 * @return true on a valid reading, false if the sensor returns NaN.
 */
bool dht22_read_temperature(float *out);

/**
 * @brief Read relative humidity from the DHT22.
 *
 * The Adafruit DHT library caches the last measurement, so calling this
 * immediately after dht22_read_temperature() does not trigger a second
 * hardware read.
 *
 * @param[out] out  Relative humidity in percent (0–100).
 * @return true on a valid reading, false if the sensor returns NaN.
 */
bool dht22_read_humidity(float *out);
