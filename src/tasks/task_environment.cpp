#include <Arduino_FreeRTOS.h>
#include <semphr.h>

#include "shared.h"
#include "bh1750.h"
#include "pir.h"

// Task wakes every 250 ms to poll the PIR.
static constexpr TickType_t TASK_PERIOD    = pdMS_TO_TICKS(250);
// BH1750 is read once per second (every 4 task wakes).
static constexpr TickType_t LIGHT_INTERVAL = pdMS_TO_TICKS(1000);

// Suppress PIR readings for 60 s after power-on (HC-SR501 warm-up).
static constexpr uint32_t PIR_WARMUP_TICKS = pdMS_TO_TICKS(60000UL);

void task_environment(void *pvParameters) {
    (void)pvParameters;

    TickType_t last_light_read = xTaskGetTickCount();

    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        const uint16_t ts =
            static_cast<uint16_t>(now / configTICK_RATE_HZ);

        // --- PIR (every wake, after warm-up) ---
        if (now >= PIR_WARMUP_TICKS) {
            float motion;
            if (pir_read(&motion) && motion > 0.5f) {
                sensor_reading_t reading;
                reading.type        = SensorType::MOTION;
                reading.value       = motion;
                reading.timestamp_s = ts;
                reading._padding    = 0;
                xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
            }
        }

        // --- BH1750 (every 1 s) ---
        if ((now - last_light_read) >= LIGHT_INTERVAL) {
            last_light_read = now;

            if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                float lux;
                if (bh1750_read(&lux)) {
                    sensor_reading_t reading;
                    reading.type        = SensorType::LIGHT;
                    reading.value       = lux;
                    reading.timestamp_s = ts;
                    reading._padding    = 0;
                    xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
                }
                xSemaphoreGive(i2c_mutex);
            }
        }

        vTaskDelay(TASK_PERIOD);
    }
}
