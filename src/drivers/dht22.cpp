#include "drv_dht22.h"
#include "../config/pins.h"

#include <Arduino.h>

static float s_temp    = 0.0f;
static float s_hum     = 0.0f;
static bool  s_last_ok = false;

// Wait for pin to reach `level`, return false if it takes longer than timeout_us.
static bool wait_for(uint8_t pin, uint8_t level, uint32_t timeout_us) {
    uint32_t start = micros();
    while (digitalRead(pin) != level) {
        if ((micros() - start) >= timeout_us) return false;
    }
    return true;
}

// Measure how long pin stays at `level` (µs). Returns 0 on timeout.
static uint32_t measure_pulse(uint8_t pin, uint8_t level, uint32_t timeout_us) {
    uint32_t start = micros();
    while (digitalRead(pin) == level) {
        if ((micros() - start) >= timeout_us) return 0;
    }
    return micros() - start;
}

static bool read_sensor(float *temp, float *hum) {
    uint8_t data[5] = {0};

    // Send start signal: pull low for 1 ms, then release.
    pinMode(PIN_DHT22, OUTPUT);
    digitalWrite(PIN_DHT22, LOW);
    delay(1);
    pinMode(PIN_DHT22, INPUT_PULLUP);

    // Sensor responds with 80 µs LOW then 80 µs HIGH before sending data.
    if (!wait_for(PIN_DHT22, LOW,  100)) return false; // wait for sensor to pull LOW
    if (!wait_for(PIN_DHT22, HIGH, 150)) return false; // wait for LOW to end (~80 µs)
    if (!wait_for(PIN_DHT22, LOW,  150)) return false; // wait for HIGH to end (~80 µs)

    // Read 40 bits. Each bit: 50 µs LOW start pulse, then HIGH whose duration
    // encodes the value (< 50 µs = 0, > 50 µs = 1).
    for (uint8_t i = 0; i < 40; i++) {
        if (!wait_for(PIN_DHT22, HIGH, 100)) return false; // wait for 50 µs LOW to end
        uint32_t high_us = measure_pulse(PIN_DHT22, HIGH, 150);
        if (high_us == 0) return false;

        data[i / 8] <<= 1;
        if (high_us > 50) data[i / 8] |= 1;
    }

    // Checksum: last byte must equal lower 8 bits of sum of first four.
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return false;

    *hum  = ((data[0] << 8) | data[1]) * 0.1f;
    *temp = (((data[2] & 0x7F) << 8) | data[3]) * 0.1f;
    if (data[2] & 0x80) *temp = -*temp;

    return true;
}

bool dht22_init() {
    pinMode(PIN_DHT22, INPUT_PULLUP);
    return true;
}

bool dht22_read_temperature(float *out) {
    s_last_ok = read_sensor(&s_temp, &s_hum);
    if (!s_last_ok) return false;
    *out = s_temp;
    return true;
}

bool dht22_read_humidity(float *out) {
    if (!s_last_ok) return false;
    *out = s_hum;
    return true;
}
