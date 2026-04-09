# IoT Environmental Monitoring Station

> FreeRTOS firmware for Arduino Uno R3 — concurrent multi-sensor environmental monitoring over Serial. (Work in progress)

![Platform](https://img.shields.io/badge/platform-Arduino%20Uno%20R3-00979D?logo=arduino&logoColor=white)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Build](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

---

## Overview

This project implements a real-time environmental monitoring station on an **Arduino Uno R3 (ATmega328P)** using **FreeRTOS**. Three sensors run concurrently across independent tasks — temperature & humidity, light intensity, and motion — and report structured data through a single Serial output task.

Designed with a constrained 2KB SRAM budget in mind, the architecture balances task isolation, safe inter-task communication via queues, and correct I2C resource management with a mutex.

---

## Sensors

| Sensor | Measures | Interface | Pin |
|--------|----------|-----------|-----|
| DHT22 / AM2302 | Temperature + Humidity | One-wire | Digital 2 |
| BH1750FVI | Light intensity (lux) | I2C | SDA=A4, SCL=A5 |
| HC-SR501 PIR | Motion detection | Digital GPIO | Digital 3 |

---

## Sample Output

```
[00042] TEMP:   23.50 C
[00042] HUM:    61.20 %
[00043] LIGHT:  342 lux
[00043] MOTION: detected
```

Format: `[uptime_seconds] TYPE: value unit`

---

## Tech Stack

| | |
|---|---|
| **Language** | C++17 |
| **RTOS** | FreeRTOS `feilipu/FreeRTOS@^11.1.0` |
| **Build system** | PlatformIO |
| **Board** | Arduino Uno R3 — ATmega328P, 2KB SRAM, 32KB Flash |
| **Sensor libs** | `adafruit/DHT sensor library`, `adafruit/Adafruit Unified Sensor`, `claws/BH1750` |

---

## Getting Started

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```bash
# Build
pio run

# Upload to board
pio run --target upload

# Open Serial Monitor (115200 baud)
pio device monitor --baud 115200
```

See [docs/wiring.md](docs/wiring.md) for the full wiring diagram.

---

## Project Structure

```
iot-monitoring-station/
├── platformio.ini
├── include/
│   ├── types.h          # sensor_reading_t, SensorType
│   ├── shared.h         # extern queue + mutex handles
│   ├── dht22.h
│   ├── bh1750.h
│   └── pir.h
├── src/
│   ├── main.cpp         # FreeRTOS setup, task creation, scheduler start
│   ├── config/
│   │   └── pins.h
│   ├── drivers/
│   │   ├── dht22.cpp
│   │   ├── bh1750.cpp
│   │   └── pir.cpp
│   └── tasks/
│       ├── task_dht22.cpp
│       ├── task_environment.cpp
│       └── task_serial.cpp
└── docs/
    └── wiring.md
```

---

## License

[MIT](LICENSE) — DiogoFSPinheiro
