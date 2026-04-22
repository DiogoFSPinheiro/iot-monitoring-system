#include "drv_bh1750.h"

static BH1750 light_meter;

bool bh1750_init() {
    // BH1750::begin() calls Wire.begin() internally; safe to call after
    // Wire.begin() has already been called (no-op on re-entry).
    return light_meter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
}

bool bh1750_read(float *out) {
    const float lux = light_meter.readLightLevel();
    if (lux < 0.0f) {
        return false;
    }
    *out = lux;
    return true;
}
