#pragma once

#include <stdint.h>

// One-wire sensor
constexpr uint8_t PIN_DHT22 = 2;

// PIR motion sensor (digital GPIO)
constexpr uint8_t PIN_PIR = 3;

// BH1750FVI uses hardware I2C: SDA = A4, SCL = A5 (fixed on Uno R3)
