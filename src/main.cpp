#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <avr/wdt.h>
#include <queue.h>
#include <semphr.h>

#include "config/pins.h"
#include "shared.h"
#include "drv_dht22.h"
#include "drv_bh1750.h"
#include "drv_pir.h"

// Runs before main() — disables WDT left enabled by Optiboot bootloader.
void wdt_init() __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init() {
    MCUSR = 0;
    wdt_disable();
    __asm__ __volatile__ ("ret");
}


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

    // --- Create FreeRTOS shared objects ---
    sensor_data_queue = xQueueCreate(4, sizeof(sensor_reading_t));
    i2c_mutex         = xSemaphoreCreateMutex();

    if (!sensor_data_queue || !i2c_mutex) {
        Serial.println(F("FATAL: FreeRTOS object creation failed"));
        while (true) {}
    }

    // --- Initialise drivers ---
    dht22_init();
    pir_init();

    if (!bh1750_init()) {
        Serial.println(F("WARN: BH1750 init failed — check wiring"));
    }

    // --- Create tasks ---
    BaseType_t t1 = xTaskCreate(task_serial,      "serial", 160, nullptr, 3, nullptr);
    BaseType_t t2 = xTaskCreate(task_dht22,       "dht22",  128, nullptr, 2, nullptr);
    BaseType_t t3 = xTaskCreate(task_environment, "env",    128, nullptr, 2, nullptr);

    if (t1 != pdPASS || t2 != pdPASS || t3 != pdPASS) {
        Serial.println(F("FATAL: task creation failed (out of memory)"));
        while (true) {}
    }

    Serial.print(F("Free RAM after RTOS init:  "));
    Serial.print(free_ram());
    Serial.println(F(" bytes"));
    Serial.println(F("Scheduler starting..."));
    Serial.flush();

    // FreeRTOS scheduler starts automatically when setup() returns
    // (feilipu/FreeRTOS Arduino library behaviour).
}

void loop() {
    // Never reached — the FreeRTOS idle task runs here instead.
}
