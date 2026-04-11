#pragma once

#include <stdbool.h>

/**
 * @brief Initialise the HC-SR501 PIR sensor pin.
 *
 * Configures the GPIO pin as INPUT.  Note: the HC-SR501 requires ~60 s
 * warm-up time after power-on before readings are reliable.  The task is
 * responsible for suppressing readings during that window.
 *
 * Call once from setup() before the FreeRTOS scheduler starts.
 */
void pir_init();

/**
 * @brief Read the HC-SR501 PIR motion sensor.
 *
 * This is a plain digitalRead() and cannot fail.
 *
 * @param[out] out  1.0f if motion is detected, 0.0f otherwise.
 * @return true always.
 */
bool pir_read(float *out);
