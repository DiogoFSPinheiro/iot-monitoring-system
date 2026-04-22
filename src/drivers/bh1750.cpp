#include "drv_bh1750.h"

#include <BH1750.h>
#include <Wire.h>

static BH1750 light_meter;

bool bh1750_init() {
    // BH1750::begin() calls Wire.begin() internally; safe to call after
    // Wire.begin() has already been called (no-op on re-entry).
    return light_meter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
}

bool bh1750_read(float *out) {
    const float lux = light_meter.readLightLevel();
    if (lux < 0.0f) {
        // Clear the timeout flag so subsequent reads are not poisoned.
        // Wire.setWireTimeout(, true) already reset the TWI hardware;
        // BH1750 recovers on its own once the bus goes idle.
        Wire.clearWireTimeoutFlag();
        return false;
    }
    *out = lux;
    return true;
}
