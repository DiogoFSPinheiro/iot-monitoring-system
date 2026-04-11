#pragma once

#include <stdint.h>

/**
 * @brief Identifies which sensor produced a reading.
 */
enum class SensorType : uint8_t {
    TEMPERATURE = 0x01,
    HUMIDITY    = 0x02,
    LIGHT       = 0x03,
    MOTION      = 0x04,
};

/**
 * @brief Single sensor reading passed through the inter-task queue.
 *
 * Kept at 8 bytes to minimise queue RAM on the 2KB Uno SRAM budget.
 */
struct sensor_reading_t {
    SensorType type;       // 1 byte
    float      value;      // 4 bytes
    uint16_t   timestamp_s;// 2 bytes — seconds since boot (wraps at ~18 h)
    uint8_t    _padding;   // 1 byte  — explicit alignment pad
};                         // Total: 8 bytes
