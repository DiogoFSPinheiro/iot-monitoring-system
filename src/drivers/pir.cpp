#include "pir.h"
#include "../config/pins.h"

#include <Arduino.h>

void pir_init() {
    pinMode(PIN_PIR, INPUT);
}

bool pir_read(float *out) {
    *out = (digitalRead(PIN_PIR) == HIGH) ? 1.0f : 0.0f;
    return true;
}
