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

| Sensor | Measures | Interface | Pin | Driver |
|--------|----------|-----------|-----|--------|
| DHT22 / AM2302 | Temperature + Humidity | One-wire | Digital 2 | Bare metal (AVR registers + Timer1) |
| BH1750FVI | Light intensity (lux) | I2C | SDA=A4, SCL=A5 | `claws/BH1750` library |
| HC-SR501 PIR | Motion detection | Digital GPIO | Digital 3 | Bare metal (AVR registers) |

The **DHT22** and **PIR** drivers bypass all Arduino abstractions (`digitalRead`, `pinMode`, `delay`) and operate directly on AVR I/O registers (`DDRx`, `PORTx`, `PINx`). The DHT22 driver also configures Timer1 as a free-running 2 MHz counter for µs-accurate pulse timing without `micros()`. The **BH1750** driver wraps the `claws/BH1750` library, which handles the I2C protocol through the Arduino `Wire` API.

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
| **RTOS** | FreeRTOS `feilipu/FreeRTOS@11.1.0-3` |
| **Build system** | PlatformIO |
| **Board** | Arduino Uno R3 — ATmega328P, 2KB SRAM, 32KB Flash |
| **Libraries** | `feilipu/FreeRTOS@11.1.0-3`, `claws/BH1750@^1.3.0` |

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
iot-monitoring-system/
├── platformio.ini
├── include/
│   ├── types.h          # sensor_reading_t, SensorType
│   ├── shared.h         # extern queue + mutex handles
│   ├── drv_dht22.h
│   ├── drv_bh1750.h
│   └── drv_pir.h
├── src/
│   ├── main.cpp         # FreeRTOS setup, task creation, scheduler start
│   ├── config/
│   │   └── pins.h       # Pin + port/DDR/PIN macros — no magic numbers
│   ├── drivers/
│   │   ├── dht22.cpp    # Bare metal: AVR registers + Timer1
│   │   ├── bh1750.cpp   # claws/BH1750 library wrapper
│   │   └── pir.cpp      # Bare metal: AVR registers
│   └── tasks/
│       ├── task_dht22.cpp
│       ├── task_environment.cpp
│       └── task_serial.cpp
└── docs/
    ├── wiring.md
    └── manual/
        ├── lessons_learned.md       # 9 bugs and lessons from building this on FreeRTOS
        ├── avr_registers.md         # AVR GPIO + Timer1 bare-metal reference
        └── stack_overflow_debugging.md  # Stack overflow investigation and final HWM values
```

---

## Engineering Notes

The `docs/manual/` directory contains notes written during development — real bugs that took time to find, not after-the-fact summaries.

- **[lessons_learned.md](docs/manual/lessons_learned.md)** — 9 lessons from building FreeRTOS on the Uno: the Adafruit DHT library silently reboots the board via WDT interaction, why `pulseIn()` is unreliable for the DHT22 protocol, the DTR reset on serial monitor open, Optiboot leaving the WDT armed after upload, `vTaskSuspendAll()` making timing worse, Timer1 wraparound breaking unsigned subtraction when dividing raw ticks, and more.
- **[avr_registers.md](docs/manual/avr_registers.md)** — Reference for bare-metal GPIO and Timer1 on the ATmega328P: the three register model (`DDRx`/`PORTx`/`PINx`), bit manipulation primitives, the pin macro pattern used in this project, and Timer1 prescaler/overflow reference.
- **[stack_overflow_debugging.md](docs/manual/stack_overflow_debugging.md)** — The full stack overflow investigation when BH1750 was added: three silent crashes, how `uxTaskGetStackHighWaterMark` was used to find them, the fix (moving `char[]` buffers to `static`), and the final measured HWM values for all tasks.

---

## License

[MIT](LICENSE) — DiogoFSPinheiro
