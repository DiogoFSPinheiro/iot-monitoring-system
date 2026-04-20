#include <Arduino.h>
#include <Arduino_FreeRTOS.h>

#include "shared.h"
#include "drv_dht22.h"

// Read interval: DHT22 datasheet max sampling rate is 0.5 Hz (1 read / 2 s).
static constexpr TickType_t READ_INTERVAL = pdMS_TO_TICKS(2000);

void task_dht22(void *pvParameters) {
    (void)pvParameters;

    vTaskDelay(pdMS_TO_TICKS(2000)); // let scheduler settle + DHT22 warm-up

    for (;;) {
        const uint16_t ts =
            static_cast<uint16_t>(xTaskGetTickCount() / configTICK_RATE_HZ);

        sensor_reading_t reading;
        reading.timestamp_s = ts;
        reading._padding    = 0;

        float temp = NAN;
        bool temp_ok = dht22_read_temperature(&temp);
        float hum = NAN;
        bool hum_ok = dht22_read_humidity(&hum);

        if (!temp_ok) {
            Serial.println(F("[DBG] DHT22 read failed — check wiring (VCC, GND, DATA pin 2, pull-up)"));
        }

        if (temp_ok) {
            reading.type  = SensorType::TEMPERATURE;
            reading.value = temp;
            xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
        }
        if (hum_ok) {
            reading.type  = SensorType::HUMIDITY;
            reading.value = hum;
            xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
        }

        vTaskDelay(READ_INTERVAL);
    }
}
