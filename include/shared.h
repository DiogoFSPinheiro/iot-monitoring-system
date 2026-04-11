#pragma once

#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

#include "types.h"

/**
 * @brief Queue shared between sensor tasks and task_serial.
 *
 * Capacity: 8 items × sizeof(sensor_reading_t) = 64 bytes.
 * Defined in main.cpp.
 */
extern QueueHandle_t sensor_data_queue;

/**
 * @brief Mutex protecting the I2C bus (Wire).
 *
 * Must be taken before any Wire transaction and released immediately after.
 * Defined in main.cpp.
 */
extern SemaphoreHandle_t i2c_mutex;
