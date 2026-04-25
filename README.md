# IoT Environmental Monitoring Station

> FreeRTOS firmware for Arduino Uno R3 — concurrent multi-sensor environmental monitoring over Serial.

![Platform](https://img.shields.io/badge/platform-Arduino%20Uno%20R3-00979D?logo=arduino&logoColor=white)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Build](https://img.shields.io/badge/build-PlatformIO-orange?logo=platformio)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

---

## Overview

This project implements a real-time environmental monitoring station on an **Arduino Uno R3 (ATmega328P)** using **FreeRTOS**. Three sensors run concurrently across independent tasks — temperature & humidity, light intensity, and motion — and report structured data through a single Serial output task.

Designed with a constrained 2KB SRAM budget in mind, the architecture balances task isolation, safe inter-task communication via queues, and correct I2C resource management with a mutex.

All three sensor drivers are written at the bare-metal level — no Arduino sensor libraries are used. The DHT22 and PIR drivers operate directly on AVR GPIO registers (`DDRx`, `PORTx`, `PINx`). The BH1750 driver drives the ATmega328P TWI hardware registers (`TWCR`, `TWSR`, `TWBR`, `TWDR`) directly, bypassing the Arduino `Wire` library entirely. The DHT22 driver also configures Timer1 as a free-running 2 MHz counter for µs-accurate pulse timing without `micros()`.

---

## Sensors

| Sensor | Measures | Interface | Pin | Driver implementation |
|--------|----------|-----------|-----|-----------------------|
| DHT22 / AM2302 | Temperature + Humidity | One-wire | Digital 2 | Bare metal — AVR GPIO registers + Timer1 |
| BH1750FVI | Light intensity (lux) | I2C | SDA=A4, SCL=A5 | Bare metal — AVR TWI hardware registers |
| HC-SR501 PIR | Motion detection | Digital GPIO | Digital 3 | Bare metal — AVR GPIO registers |

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
| **External libraries** | `feilipu/FreeRTOS@11.1.0-3` only — no sensor libraries |

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
│   ├── drv_bh1750.h     # TWI constants, timeout config
│   └── drv_pir.h
├── src/
│   ├── main.cpp         # WDT disable, FreeRTOS setup, task creation, scheduler start
│   ├── config/
│   │   └── pins.h       # Port/DDR/PIN macros for DHT22 and PIR — no magic numbers
│   ├── drivers/
│   │   ├── dht22.cpp    # Bare metal: AVR GPIO registers + Timer1 pulse timing
│   │   ├── bh1750.cpp   # Bare metal: AVR TWI hardware registers (no Wire library)
│   │   └── pir.cpp      # Bare metal: AVR GPIO registers
│   └── tasks/
│       ├── task_dht22.cpp        # Reads every 2 s, pushes TEMP + HUM to queue
│       ├── task_environment.cpp  # PIR every 250 ms (after 60 s warm-up), light every 1 s
│       └── task_serial.cpp       # Drains queue, formats and prints all sensor output
└── docs/
    ├── wiring.md
    └── manual/
        ├── lessons_learned.md          # 9 bugs and lessons from building this on FreeRTOS
        ├── avr_registers.md            # AVR GPIO + Timer1 bare-metal reference
        └── stack_overflow_debugging.md # Stack overflow investigation and final HWM values
```

---

## Engineering Notes

The `docs/manual/` directory contains notes written during development — real bugs that took time to find, not after-the-fact summaries.

- **[lessons_learned.md](docs/manual/lessons_learned.md)** — 9 lessons from building FreeRTOS on the Uno: the Adafruit DHT library silently reboots the board via WDT interaction (the intermediate fix was switching to `robtillaart/DHTNEW`; the final fix was rewriting the driver bare metal), why `pulseIn()` silently skips pulses and causes the DHT22 checksum to always fail, the DTR reset on serial monitor open, Optiboot leaving the WDT armed after upload, `vTaskSuspendAll()` making timing worse not better, Timer1 wraparound breaking unsigned subtraction when dividing raw ticks, and more.
- **[avr_registers.md](docs/manual/avr_registers.md)** — Reference for bare-metal GPIO and Timer1 on the ATmega328P: the three register model (`DDRx`/`PORTx`/`PINx`), bit manipulation primitives, the `volatile uint8_t *` requirement for register pointers, the pin macro pattern used in this project, Timer1 prescaler and overflow reference, and a full peripheral map showing which hardware each driver owns (Timer1 → DHT22, TWI → BH1750, WDT → FreeRTOS tick).
- **[avr_twi_bare_metal.md](docs/manual/avr_twi_bare_metal.md)** — Deep-dive into the bare-metal TWI/I2C driver written for BH1750: how I2C works at the wire level, the four TWI registers (`TWBR`, `TWCR`, `TWSR`, `TWDR`), every operation explained (START, SLA+W, SLA+R, ACK read, NACK read, STOP), the BH1750 init and read command sequences, why `Wire.h` was dropped, and the two bugs found while building it (Timer1 false timeouts from FreeRTOS tick resets, and the wrong status code for `SLA+R` vs `SLA+W`).
- **[stack_overflow_debugging.md](docs/manual/stack_overflow_debugging.md)** — The full stack overflow investigation when BH1750 was integrated: three silent crashes with different failure times, how `uxTaskGetStackHighWaterMark` was used to find them, why a "recovery" function made things crash faster, the fix (moving `char[]` buffers to `static` BSS), and the final measured HWM values for all tasks.
- **[acronyms.md](docs/manual/acronyms.md)** — Glossary of all abbreviations used across the manual (AVR, TWI, SLA, HWM, BSS, CTC, etc.).

---

## Documentation

API documentation is generated with [Doxygen](https://www.doxygen.nl/). From the project root:

```bash
doxygen Doxyfile
```

Then open `docs/doxygen/html/index.html` in a browser to view the generated docs.

---

## License

[MIT](LICENSE) — DiogoFSPinheiro

## Next steps

- [ ] Raspberry Pi as data receiver
- [ ] Binary serial protocol with CRC8 for Raspberry Pi communication
