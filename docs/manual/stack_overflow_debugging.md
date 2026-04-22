# Stack Overflow & I2C Lockup Debugging — Lessons Learned

This document covers the three bugs found while enabling the BH1750 light sensor
on a FreeRTOS / Arduino Uno system. Each bug was silent, took time to manifest,
and required systematic elimination to identify.

---

## Bug 1 — task_environment stack overflow (crashed at ~24s)

### Symptom
System printed sensor readings for ~24 seconds then completely froze. No output
from any task. No reset banner. `uxTaskGetStackHighWaterMark()` was not yet in
place to catch it.

### Root cause
`task_environment` was allocated **100 bytes** of stack. Before BH1750 was
enabled, the task only called `digitalRead()` (PIR) — very shallow call stack.

Enabling BH1750 added `Wire.requestFrom()` and the BH1750 library, which push
several functions deep. The task stack overflowed into adjacent memory (FreeRTOS
kernel structures or adjacent task stacks), corrupting them silently. The system
ran for ~24 seconds accumulating corruption before something critical broke.

### Fix
Increase `task_environment` stack to **128 bytes** in `xTaskCreate`.

### Key lesson
Stack overflow on AVR FreeRTOS is **silent by default** — no exception, no log.
The task just corrupts whatever lives next to it in RAM. The crash appears
unrelated and delayed, making this very hard to diagnose without HWM checks.

---

## Bug 2 — AVR FreeRTOS stack unit is BYTES, not words

### Symptom
CLAUDE.md documented stack sizes as "128 words = 256 bytes" but the actual
allocations in `xTaskCreate` were 128, 100, 130 — far less than intended.

### Root cause
On 32-bit FreeRTOS ports (ARM, ESP32), `StackType_t` = `uint32_t` (4 bytes),
so passing `128` to `xTaskCreate` allocates 512 bytes.

On **AVR** (ATmega328P), the feilipu/Arduino_FreeRTOS_Library defines:
```c
typedef uint8_t StackType_t;
```
So passing `128` to `xTaskCreate` allocates exactly **128 bytes**. The number
IS the byte count, directly.

### Key lesson
Always check `portSTACK_TYPE` / `StackType_t` for your specific FreeRTOS port.
Never assume the "words" convention from documentation written for 32-bit systems.

**Rule of thumb for AVR:** xTaskCreate depth = bytes. Period.

---

## Bug 3 — task_serial dtostrf stack overflow (crashed at ~30s)

### Symptom
System ran for ~30 seconds then froze completely — all tasks silent, no output.
`uxTaskGetStackHighWaterMark()` for task_serial was never collected because
the crash happened before the measurement stabilised.

### Root cause
`task_serial` was allocated **130 bytes**. Stack usage breakdown:

```
FreeRTOS context save (registers + PC + SREG):  ~35 bytes
Local variables:
  sensor_reading_t reading                         8 bytes
  char line[40]                                   40 bytes
  char fval[10]                                   10 bytes
                                                 ─────────
Subtotal locals:                                  58 bytes

Available for ALL function calls:    130 - 35 - 58 = 37 bytes
```

`dtostrf()` (float → string conversion) on AVR calls deep into the avr-libc
float library (`__dtoa_engine` and friends). It needs **80–120 bytes** of call
stack depth alone. Every single float print was overflowing by ~50–80 bytes,
silently corrupting adjacent memory.

### Why it took 30 seconds to crash
The corruption was not immediate. Each call corrupted a little more. The system
accumulated ~15 reads × 2 floats = 30 stack overflows before a critical structure
(FreeRTOS TCB, queue handle, or kernel data) was overwritten enough to break
execution.

### Fix — two-part

**Part 1:** Move the large buffers out of the task stack into BSS (static storage):
```cpp
// In task_serial — these stay allocated permanently in BSS, but cost 0 stack.
static char line[40];
static char fval[10];
```
This frees 50 bytes of stack for function call chains.

**Part 2:** Increase task_serial stack to **160 bytes**.

After both fixes, measured HWM = **83 bytes remaining** — comfortable headroom.

### Key lesson
`dtostrf()` on AVR is stack-hungry. Any task that formats floats needs generous
stack. Moving large local buffers to `static` is a valid and common AVR technique
to reduce stack pressure — safe as long as the task is a singleton (which FreeRTOS
tasks always are).

---

## Bug 4 — BH1750 I2C bus lockup (caused freeze after recovery)

### Symptom
Even after fixing the stack issues, the system occasionally froze. Output would
stop mid-run with no crash banner. Sometimes `LIGHT` readings would be missing
for 2–3 seconds and then return; other times the freeze was permanent.

### Root cause
The Arduino Wire library on AVR uses the hardware TWI (Two-Wire Interface)
peripheral. `Wire.requestFrom()` waits in a busy-loop for the TWINT flag
(TWI interrupt flag) to be set by the hardware. If the I2C bus gets into a bad
state (noise, glitch, sensor not responding), TWINT never fires and the call
blocks **forever**. The task holds `i2c_mutex` while blocked, and `task_serial`
has nothing to print — the whole system appears frozen.

### Fix
Call `Wire.setWireTimeout()` once in `setup()`:
```cpp
Wire.begin();
Wire.setWireTimeout(3000 /* µs */, true /* reset TWI hardware on timeout */);
```
After 3ms with no response, Wire gives up, resets the TWI peripheral, and
returns. `bh1750_read()` returns `false`, the mutex is released, and the task
continues normally. Missing a single light reading is far better than freezing.

In `bh1750_read()`, clear the timeout flag so subsequent reads are not poisoned:
```cpp
bool bh1750_read(float *out) {
    const float lux = light_meter.readLightLevel();
    if (lux < 0.0f) {
        Wire.clearWireTimeoutFlag();
        return false;
    }
    *out = lux;
    return true;
}
```

### What NOT to do — the failed I2C bus recovery attempt
A "proper" I2C bus recovery (9 SCL clock pulses to force the slave to release SDA,
then a STOP condition, then Wire.begin()) was implemented but made things **worse**:

```cpp
// This looked correct but crashed the system faster:
static void recover_i2c_bus() {
    TWCR = 0;
    pinMode(PIN_SDA, INPUT_PULLUP);
    pinMode(PIN_SCL, OUTPUT);
    for (uint8_t i = 0; i < 9; ++i) { /* bit-bang SCL */ }
    Wire.begin();
    Wire.setWireTimeout(3000, true);
}
```

The recovery function called `pinMode`, `digitalWrite`, and `Wire.begin()` — a
deep call chain that itself caused a stack overflow in `task_environment` (which
still had its wire-reading call on the stack at that point). System crashed at
~8 seconds instead of ~30.

**Lesson:** A recovery path that uses more stack than the normal path can make
things catastrophically worse on memory-constrained systems. Measure first.

### Key lesson
Always set `Wire.setWireTimeout()` on systems where I2C is critical. The default
(no timeout) means a single bad transaction hangs the entire task forever. 3ms
is enough for any standard I2C transaction at 100kHz.

---

## Debugging Methodology

### Tools used

**`uxTaskGetStackHighWaterMark(nullptr)`**
Returns the minimum number of bytes that were ever free on the task's stack
(the "watermark" — lowest the stack has ever been). Call it after the task has
done its most complex operation at least once.

```cpp
Serial.print(F("[DBG] stack HWM (bytes): "));
Serial.println(uxTaskGetStackHighWaterMark(nullptr));
```
A value near zero means you are about to overflow. A value of 20–40 bytes is
dangerously low. Aim for at least 30–40 bytes of headroom on AVR.

**`free_ram()` helper in setup()**
```cpp
static uint16_t free_ram() {
    extern int __heap_start;
    extern int *__brkval;
    int v;
    return static_cast<uint16_t>(
        reinterpret_cast<int>(&v) -
        (__brkval == nullptr
             ? reinterpret_cast<int>(&__heap_start)
             : reinterpret_cast<int>(__brkval)));
}
```
Measures the gap between the heap top and the main stack. Useful for catching
out-of-memory before the scheduler starts. Measure before and after RTOS init.

### Diagnostic pattern

| Symptom | Likely cause |
|---|---|
| Freeze after N seconds, no reset banner | Stack overflow accumulating corruption |
| Freeze immediately, L LED blinking fast | Out of memory — task creation failed silently or scheduler crashed |
| Freeze after N seconds, then restart banner | WDT reset (check interrupt-disabled duration) |
| LIGHT readings missing for a few seconds, then back | Wire I2C timeout firing and recovering |
| LIGHT missing permanently + all output stops | Wire blocked forever (no timeout set) |

### Order of investigation
1. Add HWM prints to ALL tasks — measure after the first real work cycle.
2. Check `free_ram()` before and after RTOS init — budget for idle task too.
3. If HWM is below 30 bytes, increase stack before investigating further.
4. Add a 5s `xQueueReceive` timeout with a heartbeat print in task_serial — if
   the heartbeat fires, task_serial is alive but starved of data (producer hung).
5. Use `Wire.setWireTimeout()` from the start on any I2C project.

---

## Final Stack Configuration

```
Task               Stack (bytes)   HWM remaining   Notes
─────────────────────────────────────────────────────────────────
task_serial        160             83              line[]/fval[] moved to static
task_dht22         128             61
task_environment   128             61
Idle task          92              (not measured)  configMINIMAL_STACK_SIZE
```

Free RAM after all task creation: ~680 bytes.
