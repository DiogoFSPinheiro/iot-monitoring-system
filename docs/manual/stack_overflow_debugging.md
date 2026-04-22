# Stack Overflow Debugging — Lessons Learned

This document covers the bugs found while enabling the BH1750 light sensor on a
FreeRTOS / Arduino Uno system. Each bug was silent, took time to manifest, and
required systematic elimination to identify.

---

## Root cause — everything was the same problem

All crashes had a single underlying cause: **stack sizes were specified in bytes
but mistakenly treated as words**, leaving every task with far less stack than
intended. The crashes appeared at different times depending on how quickly each
task's overflow corrupted critical memory.

---

## Bug 1 — task_environment stack too small (crashed at ~24s)

### Symptom
System printed sensor readings for ~24 seconds then completely froze. No output
from any task. No reset banner.

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

## Bug 3 — task_serial stack too small (crashed at ~30s)

### Symptom
System ran for ~30 seconds then froze completely — all tasks silent, no output.

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

With only 37 bytes left for function calls (`xQueueReceive`, `dtostrf`,
`snprintf`, `Serial.println`), the stack was overflowing into adjacent memory
on every float print cycle. The crash at ~30 seconds was the same root cause as
Bug 1 — too few bytes.

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
Moving large local `char[]` buffers to `static` is a valid AVR technique to
reduce stack pressure — safe as long as the task is a singleton (which FreeRTOS
tasks always are). It trades stack bytes for BSS bytes.

---

## What was NOT a bug — Wire.setWireTimeout

During debugging, missing LIGHT readings were observed and a Wire I2C lockup
was suspected. `Wire.setWireTimeout(3000, true)` was added to prevent
`Wire.requestFrom()` from blocking forever.

**Tested and disproved:** After removing `Wire.setWireTimeout`, the system ran
stably for 900+ seconds. The missing LIGHT readings and freezes were caused
entirely by the stack overflows above, not by an I2C hardware issue.

`Wire.setWireTimeout` is still a reasonable **defensive measure** for production
I2C code, but it was not needed to fix the crashes in this project.

---

## What made debugging harder — the failed I2C recovery attempt

While chasing the suspected I2C lockup, a full bus recovery function was added
to `bh1750_read()`:

```cpp
// This looked correct but crashed the system faster (at ~8s):
static void recover_i2c_bus() {
    TWCR = 0;
    pinMode(PIN_SDA, INPUT_PULLUP);
    pinMode(PIN_SCL, OUTPUT);
    for (uint8_t i = 0; i < 9; ++i) { /* bit-bang SCL */ }
    Wire.begin();
    Wire.setWireTimeout(3000, true);
}
```

This function called `pinMode`, `digitalWrite`, and `Wire.begin()` — a deeper
call chain than the normal read path, added on top of an already-overflowing
stack. System crashed faster, not slower. The lesson: on memory-constrained
systems, a "recovery" path that uses more stack than the happy path can make
things catastrophically worse.

---

## Debugging Methodology

### Tools used

**`uxTaskGetStackHighWaterMark(nullptr)`**
Returns the minimum number of bytes that were ever free on the task's stack.
Call it after the task has done its most complex operation at least once.

```cpp
Serial.print(F("[DBG] stack HWM (bytes): "));
Serial.println(uxTaskGetStackHighWaterMark(nullptr));
```
A value near zero means you are about to overflow. Aim for at least 30–40 bytes
of headroom on AVR.

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
out-of-memory before the scheduler starts.

### Diagnostic pattern

| Symptom | Likely cause |
|---|---|
| Freeze after N seconds, no reset banner | Stack overflow accumulating corruption |
| Freeze immediately, L LED blinking fast | Out of memory — scheduler failed to start |
| Freeze after N seconds, then restart banner | WDT reset (check interrupt-disabled duration) |

### Order of investigation
1. Add HWM prints to ALL tasks — measure after the first real work cycle.
2. Check `free_ram()` before and after RTOS init.
3. If HWM is below 30 bytes, increase stack before investigating anything else.
4. Add a 5s `xQueueReceive` timeout with a heartbeat print in task_serial — if
   the heartbeat fires, task_serial is alive but starved of data (producer hung).

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
