#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <stdlib.h>   // dtostrf

#include "shared.h"

// dtostrf scratch buffer: sign + 5 digits + '.' + 2 decimals + NUL = 10 chars
static constexpr uint8_t FLOAT_BUF_LEN = 10;

// Full output line: "[00000] MOTION: detected\0" = 26 chars max; 40 is safe.
static constexpr uint8_t LINE_BUF_LEN = 40;

void task_serial(void *pvParameters) {
    (void)pvParameters;

    sensor_reading_t reading;
    char line[LINE_BUF_LEN];
    char fval[FLOAT_BUF_LEN];

    for (;;) {
        // Block indefinitely until a reading arrives.
        if (xQueueReceive(sensor_data_queue, &reading, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (reading.type) {
            case SensorType::TEMPERATURE:
                dtostrf(reading.value, 5, 2, fval);
                snprintf(line, sizeof(line), "[%05u] TEMP:   %s C",
                         reading.timestamp_s, fval);
                break;

            case SensorType::HUMIDITY:
                dtostrf(reading.value, 5, 2, fval);
                snprintf(line, sizeof(line), "[%05u] HUM:    %s %%",
                         reading.timestamp_s, fval);
                break;

            case SensorType::LIGHT:
                dtostrf(reading.value, 5, 0, fval);
                snprintf(line, sizeof(line), "[%05u] LIGHT:  %s lux",
                         reading.timestamp_s, fval);
                break;

            case SensorType::MOTION:
                snprintf(line, sizeof(line), "[%05u] MOTION: detected",
                         reading.timestamp_s);
                break;

            default:
                continue;
        }

        Serial.println(line);
    }
}
