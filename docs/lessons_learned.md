# Lessons Learned — IoT Environmental Monitor

Things I ran into while building a FreeRTOS project on an Arduino Uno R3 that took hours to debug.
Sharing so others don't lose the same time.

---

## 1. The Adafruit DHT22 library silently reboots your board when using FreeRTOS

The Adafruit DHT library disables interrupts (`cli()`) for up to **80ms** while bit-banging the one-wire protocol.

feilipu/FreeRTOS uses the hardware Watchdog Timer (WDT) as its tick source, configured in **interrupt+reset mode** with a 15ms timeout. The WDT ISR re-arms itself on every tick. But with interrupts globally disabled:

- At **t=15ms** — WDT fires, interrupt is pending but blocked by `cli()`, the interrupt-enable bit is auto-cleared
- At **t=30ms** — WDT fires again, interrupt-enable is already cleared, reset-enable is still armed → **silent hardware reset**

The board reboots every time the DHT22 task runs. No crash output, no error message — just a silent restart.

**Fix:** Replace the Adafruit DHT library with `robtillaart/DHTNEW` and call `dht.setDisableIRQ(false)`. DHTNEW reads the sensor without ever calling `cli()`, so the WDT ISR fires normally and the scheduler is never disrupted.

---

## 2. The serial monitor resets your Arduino on first connection

PlatformIO's serial monitor toggles the DTR line when it opens. On Arduino Uno, DTR is wired through a capacitor to the RESET pin. This resets the board right as the FreeRTOS scheduler starts — producing garbage bytes in the output. The second boot works fine because DTR is already stable.

**Fix:** Add this to `platformio.ini`:
```ini
monitor_dtr = 0
monitor_rts = 0
```

---

## 3. Optiboot leaves the Watchdog Timer running after upload

The Optiboot bootloader enables the WDT during the upload process and doesn't always clean up after itself. Combined with FreeRTOS reconfiguring the WDT at scheduler start, this causes a crash on first boot after every upload.

**Fix:** Disable the WDT in the AVR `.init3` startup section — this runs before `main()`, before any C++ constructors, before anything else:

```cpp
#include <avr/wdt.h>

void wdt_init() __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init() {
    MCUSR = 0;
    wdt_disable();
    __asm__ __volatile__ ("ret");
}
```

---

## 4. `vTaskSuspendAll()` made the timing problem worse, not better

My first instinct was to wrap the DHT read in `vTaskSuspendAll()` / `xTaskResumeAll()` to protect FreeRTOS from the long interrupt-disabled period. It made things worse.

With the scheduler suspended, FreeRTOS cannot count ticks that arrive during the suspension. After `xTaskResumeAll()`, the tick clock is severely behind real time. A queue timeout configured for 5 seconds was taking 15+ seconds to fire.

**Lesson:** `vTaskSuspendAll()` is for protecting short critical sections from context switches — not for wrapping long interrupt-disabled operations. Fix the root cause (the library) instead.

---

## 5. Sensor library labels don't always match standard names

The BH1750 module I used labels its pins **DAT** and **CSL** instead of the standard **SDA** and **SCL**. Same pins, non-standard labels.

- **DAT = SDA → A4** on Arduino Uno
- **CSL = SCL → A5** on Arduino Uno

The ADDR pin must be tied to GND to set I2C address `0x23`. If it floats, the sensor won't respond and init hangs — which on this FreeRTOS setup triggers the WDT reset loop.

---

## 6. The FreeRTOS idle task stack is larger than you expect

`configMINIMAL_STACK_SIZE` set in `platformio.ini` build flags gets **overridden** by the feilipu library's own `FreeRTOSConfig.h` (to 192 words = 384 bytes). The idle task is also created *after* `setup()` returns — so the free RAM measurement printed at the end of `setup()` doesn't include it.

Budget roughly **420 bytes** for the idle task (stack + TCB) on top of whatever `free_ram()` reports at scheduler start.

---

## 7. Only one task should ever call `Serial.print()`

Calling `Serial.print()` from multiple FreeRTOS tasks causes output corruption. The right pattern: all tasks send data to a shared queue, and a single dedicated `task_serial` drains the queue and does all printing. No mutex on `Serial` needed because only one task ever touches it.

---

## 8. `pulseIn()` silently skips pulses when the pin is already in the target state

`pulseIn(pin, LOW)` doesn't measure the pulse the pin is currently in. It first waits for the pin to go HIGH, then waits for it to go LOW again — meaning if the pin is already LOW, it skips the current pulse and measures the next one.

On the DHT22 protocol, the sensor starts pulling the data line LOW about 20µs after the host releases it. With a `delayMicroseconds(40)` before calling `pulseIn(LOW)`, the sensor has already started its 80µs LOW handshake pulse. `pulseIn` skips it, waits for the next LOW — which is the first data bit. Every subsequent bit is then off by one, and the checksum always fails.

The bug is silent: `read_sensor()` just returns false every time, with no indication of why.

**Fix:** Replace `pulseIn()` with `micros()`-based helpers that work correctly regardless of the pin's current state:

```cpp
static bool wait_for(uint8_t pin, uint8_t level, uint32_t timeout_us) {
    uint32_t start = micros();
    while (digitalRead(pin) != level)
        if ((micros() - start) >= timeout_us) return false;
    return true;
}

static uint32_t measure_pulse(uint8_t pin, uint8_t level, uint32_t timeout_us) {
    uint32_t start = micros();
    while (digitalRead(pin) == level)
        if ((micros() - start) >= timeout_us) return 0;
    return micros() - start;
}
```

---

## Stack

- **Board:** Arduino Uno R3 (ATmega328P, 2KB SRAM)
- **RTOS:** feilipu/FreeRTOS 11.1.0
- **Build system:** PlatformIO
- **Sensors:** DHT22, BH1750, HC-SR501 PIR
