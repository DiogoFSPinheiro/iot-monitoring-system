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

    Serial.println(F("[DBG] task_serial started"));

    // Static: keeps 50 bytes off the task stack so dtostrf's deep call chain
    // has room. Safe because only task_serial ever touches these.
    static char line[LINE_BUF_LEN];
    static char fval[FLOAT_BUF_LEN];

    sensor_reading_t reading;
    bool hwm_printed = false;

    for (;;) {
        // Block up to 5 s; print heartbeat if no reading arrives (debug).
        if (xQueueReceive(sensor_data_queue, &reading, pdMS_TO_TICKS(5000)) != pdTRUE) {
            Serial.println(F("[DBG] queue empty — waiting for sensor data"));
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

        if (!hwm_printed) {
            Serial.print(F("[DBG] serial stack HWM (bytes): "));
            Serial.println(uxTaskGetStackHighWaterMark(nullptr));
            hwm_printed = true;
        }
    }
}
