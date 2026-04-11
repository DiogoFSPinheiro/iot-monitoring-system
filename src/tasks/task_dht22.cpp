#include <Arduino_FreeRTOS.h>

#include "shared.h"
#include "dht22.h"

// Read interval: DHT22 datasheet max sampling rate is 0.5 Hz (1 read / 2 s).
static constexpr TickType_t READ_INTERVAL = pdMS_TO_TICKS(2000);

void task_dht22(void *pvParameters) {
    (void)pvParameters;

    for (;;) {
        const uint16_t ts =
            static_cast<uint16_t>(xTaskGetTickCount() / configTICK_RATE_HZ);

        sensor_reading_t reading;
        reading.timestamp_s = ts;
        reading._padding    = 0;

        float temp;
        if (dht22_read_temperature(&temp)) {
            reading.type  = SensorType::TEMPERATURE;
            reading.value = temp;
            xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
        }

        // readHumidity() reuses the cached measurement from readTemperature();
        // no extra hardware read occurs here.
        float hum;
        if (dht22_read_humidity(&hum)) {
            reading.type  = SensorType::HUMIDITY;
            reading.value = hum;
            xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
        }

        vTaskDelay(READ_INTERVAL);
    }
}
