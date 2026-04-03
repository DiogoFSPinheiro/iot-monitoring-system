# Embedded — Environmental Monitoring Station

@../CLAUDE.md for full project context.

## Overview

FreeRTOS-based firmware for Arduino Uno R3 that reads 3 environmental sensors concurrently and transmits packed data to a Raspberry Pi via USB-serial.

Sensor narrative: monitor a room's temperature, humidity, light, and occupancy. PIR motion events enable correlation analysis in the backend ("someone entered → light/temp changed").

IMPORTANT: The Uno has only 2KB SRAM. Every byte counts. This constraint shapes every design decision below.

## Hardware Setup

```
Arduino Uno R3 (ATmega328P, 2KB SRAM, 32KB Flash)
├── I2C Bus (SDA=A4, SCL=A5)
│   └── BH1750FVI (light sensor, addr 0x23)
├── Digital Pin 2
│   └── DHT22 / AM2302 (temperature + humidity, one-wire protocol)
├── Digital Pin 3
│   └── HC-SR501 PIR (motion detection, digital HIGH/LOW)
├── Digital Pin 13
│   └── Built-in LED (alert indicator — no external components needed)
└── Serial (TX=1, RX=0) — shared with USB
    └── USB cable → Raspberry Pi (reads via /dev/ttyACM0 or /dev/ttyUSB0)
```

NOTE: The Uno's single hardware Serial is shared between USB and pin 0/1.
The Pi connects via the USB cable directly — no logic level converter needed.
Consequence: Serial Monitor (debug) and Pi communication use the same channel.
During development, use Serial for debug. In production, the Pi is the only reader.

## Pin Definitions

All pin numbers are defined in `src/config/pins.h`. NEVER use magic numbers in code.

## FreeRTOS Task Architecture

IMPORTANT: Only 3 tasks + idle to fit in 2KB SRAM.

```
Priority 3 (highest): task_comms       — collects sensor data from queue, checks thresholds,
                                         drives LED alert, packs and sends serial frames
Priority 2:           task_dht22       — reads temperature + humidity every 2s
Priority 2:           task_environment — reads BH1750 light (I2C) every 1s AND
                                         reads PIR motion state every 250ms
Priority 1 (lowest):  idle task        — built-in FreeRTOS idle
```

Design notes:
- BH1750 and PIR are merged into one task because PIR is a trivial digitalRead()
  and BH1750 I2C read is fast — merging saves ~128 bytes of stack
- Alert logic lives inside task_comms to avoid a 4th task
- The built-in LED (pin 13) blinks on threshold violations

### Inter-task Communication

```
task_dht22       ──→ xQueueSend(sensor_data_queue) ──→ task_comms
task_environment ──→ xQueueSend(sensor_data_queue) ──→ task_comms

I2C bus protected by: xSemaphoreTake(i2c_mutex) / xSemaphoreGive(i2c_mutex)
```

Even though only BH1750 uses I2C currently, the mutex is implemented to:
1. Demonstrate proper resource protection in RTOS (recruiter keyword)
2. Make the system extensible for future I2C sensors
3. Protect against task_comms accidentally using Wire during BH1750 reads

### Memory Budget (2048 bytes SRAM)

```
FreeRTOS kernel overhead:    ~400 bytes
task_dht22 stack:            128 words = 256 bytes
task_environment stack:      128 words = 256 bytes
task_comms stack:            150 words = 300 bytes
Idle task stack:              92 words = 184 bytes
sensor_data_queue:           8 items × 8 bytes = 64 bytes
I2C mutex:                   ~80 bytes
Global variables + buffers:  ~150 bytes
─────────────────────────────────────────────
Estimated total:             ~1690 bytes
Free headroom:               ~358 bytes
```

IMPORTANT: This is TIGHT. Do not add any more tasks or large buffers.
Always verify with `uxTaskGetStackHighWaterMark()` during development.
Use a `free_ram()` helper to monitor available SRAM at runtime.

## Data Structures

```cpp
// Shared data types — defined in include/types.h
// IMPORTANT: Keep these as small as possible (Uno RAM constraint)

enum class SensorType : uint8_t {
    TEMPERATURE = 0x01,
    HUMIDITY    = 0x02,
    LIGHT       = 0x03,
    MOTION      = 0x04,
};

struct sensor_reading_t {
    SensorType type;        // 1 byte
    float value;            // 4 bytes
    uint16_t timestamp_s;   // 2 bytes — seconds since boot (wraps at ~18h, fine for demo)
    uint8_t _padding;       // 1 byte — alignment
};                          // Total: 8 bytes per reading

enum class AlertLevel : uint8_t {
    NONE     = 0x00,
    WARNING  = 0x01,
    CRITICAL = 0x02,
};
```

## Serial Protocol

Full spec at `../docs/serial-protocol.md`.

Frame format:
```
[0xAA] [LEN] [MSG_TYPE] [PAYLOAD...] [CRC8]
```

- START_BYTE: 0xAA
- LEN: payload length (excludes start, len, type, crc)
- MSG_TYPE: 0x01 = sensor_data, 0x02 = alert, 0x03 = heartbeat
- CRC8: CRC-8/MAXIM over [MSG_TYPE + PAYLOAD]

IMPORTANT: Implement CRC in a standalone function with unit tests. Do NOT use a library for this — writing CRC by hand demonstrates embedded competence.

## Sensor Drivers

Each sensor driver lives in `src/drivers/` and exposes:
```cpp
bool sensor_init();           // returns false on failure
bool sensor_read(float* out); // returns false on failure, writes to *out
```

- NEVER block in a driver — if the sensor doesn't respond, return false
- Retry logic belongs in the task, NOT the driver

### DHT22 / AM2302 specifics
- Uses one-wire protocol on a single digital pin
- Minimum 2s between reads (datasheet specifies 0.5Hz max sampling)
- The task reads every 2s
- Returns temperature (°C) and humidity (%) as two separate queue entries
- IMPORTANT: DHT22 disables interrupts briefly during bit-banging — this is unavoidable but keep reads infrequent to minimize impact on FreeRTOS scheduler
- Range: -40°C to +80°C (temperature), 0-100% (humidity)
- Accuracy: ±0.5°C, ±2% RH — much better than DHT11

### BH1750FVI specifics
- I2C address: 0x23 (ADDR pin low) or 0x5C (ADDR pin high)
- IMPORTANT: Must acquire i2c_mutex before ANY Wire transaction
- Use continuous high-resolution mode (0x10) — 1 lux resolution, 120ms measurement time
- Returns light intensity in lux (0-65535)
- Init sequence: power on (0x01) → set mode (0x10) → wait 180ms → read
- Read: request 2 bytes, combine into uint16_t, divide by 1.2 for lux

### HC-SR501 PIR specifics
- Simplest sensor — just digitalRead()
- Returns 1.0 (motion detected) or 0.0 (no motion)
- Has a ~60s warm-up time at power-on (ignore readings during init)
- The two potentiometers on the module adjust sensitivity and hold time
- Hold time pot: how long output stays HIGH after motion (default ~2s)

## Build Configuration

platformio.ini essentials:
```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
lib_deps =
    feilipu/FreeRTOS@^11.1.0
    adafruit/DHT sensor library@^1.4.6
    adafruit/Adafruit Unified Sensor@^1.1.14
    claws/BH1750@^1.3.0
monitor_speed = 115200
build_flags =
    -D configUSE_MUTEXES=1
    -D configMINIMAL_STACK_SIZE=92
    -std=gnu++17
test_framework = unity

[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17
```

## Testing Strategy

- Unit tests run on `native` platform (no hardware needed) for:
  - CRC calculation
  - Serial frame packing/unpacking
  - Alert threshold logic
  - Data structure serialization
- Integration testing is manual on hardware with serial monitor
- Document manual test procedures in `test/MANUAL_TESTS.md`

## Common Mistakes to Avoid

- Do NOT use `String` class — it fragments the heap. Use `char[]` and `snprintf`
- Do NOT use `delay()` — use `vTaskDelay(pdMS_TO_TICKS(ms))`
- Do NOT call `Serial.print()` from multiple tasks without a mutex on Serial
- Do NOT assume sensor reads always succeed — always check return values
- Do NOT use `new` or `malloc` after scheduler starts
- Do NOT use `float` in ISRs
- Do NOT add a 4th task without recalculating the memory budget
- Do NOT use large local arrays in tasks — stack space is 128-150 words
- Do NOT forget: Serial is shared with USB. Debug prints and Pi data go through the same wire.
- Do NOT access Wire (I2C) without acquiring i2c_mutex first
- Do NOT forget the 60s PIR warm-up period — suppress motion readings during boot

## Lessons Learned
<!-- Add entries here when Claude makes mistakes -->
