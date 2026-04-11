#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include <queue.h>
#include <semphr.h>

#include "config/pins.h"
#include "shared.h"
#include "dht22.h"
#include "bh1750.h"
#include "pir.h"

// Forward declarations — implementations live in src/tasks/
void task_dht22(void *pvParameters);
void task_environment(void *pvParameters);
void task_serial(void *pvParameters);

// Definitions of the shared FreeRTOS handles (declared extern in shared.h)
QueueHandle_t     sensor_data_queue;
SemaphoreHandle_t i2c_mutex;

/**
 * @brief Returns approximate free SRAM in bytes.
 *
 * Compares the stack pointer to the top of the heap.  Useful during
 * development to verify the memory budget.
 */
static uint16_t free_ram() {
    extern int  __heap_start;
    extern int *__brkval;
    int v;
    return static_cast<uint16_t>(
        reinterpret_cast<int>(&v) -
        (__brkval == nullptr
             ? reinterpret_cast<int>(&__heap_start)
             : reinterpret_cast<int>(__brkval)));
}

void setup() {
    Serial.begin(115200);

    Serial.println(F("=== IoT Environmental Monitor ==="));
    Serial.print(F("Free RAM before RTOS init: "));
    Serial.print(free_ram());
    Serial.println(F(" bytes"));

    Wire.begin();

    // --- Create FreeRTOS shared objects ---
    sensor_data_queue = xQueueCreate(8, sizeof(sensor_reading_t));
    i2c_mutex         = xSemaphoreCreateMutex();

    if (!sensor_data_queue || !i2c_mutex) {
        Serial.println(F("FATAL: FreeRTOS object creation failed"));
        while (true) {}
    }

    // --- Initialise drivers ---
    dht22_init();
    pir_init();

    // BH1750 init requires I2C; called before scheduler so no mutex needed.
    if (!bh1750_init()) {
        Serial.println(F("WARN: BH1750 init failed — check wiring"));
    }

    // --- Create tasks ---
    // Priority 3 (highest): task_serial — drains the queue and prints output
    xTaskCreate(task_serial,      "serial", 150, nullptr, 3, nullptr);
    // Priority 2: sensor tasks
    xTaskCreate(task_dht22,       "dht22",  128, nullptr, 2, nullptr);
    xTaskCreate(task_environment, "env",    128, nullptr, 2, nullptr);

    Serial.print(F("Free RAM after RTOS init:  "));
    Serial.print(free_ram());
    Serial.println(F(" bytes"));
    Serial.println(F("Scheduler starting..."));

    // FreeRTOS scheduler starts automatically when setup() returns
    // (feilipu/FreeRTOS Arduino library behaviour).
}

void loop() {
    // Never reached — the FreeRTOS idle task runs here instead.
}
