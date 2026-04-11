#include "dht22.h"
#include "../config/pins.h"

#include <DHT.h>

static DHT dht(PIN_DHT22, DHT22);

bool dht22_init() {
    dht.begin();
    return true;
}

bool dht22_read_temperature(float *out) {
    const float val = dht.readTemperature();
    if (isnan(val)) {
        return false;
    }
    *out = val;
    return true;
}

bool dht22_read_humidity(float *out) {
    const float val = dht.readHumidity();
    if (isnan(val)) {
        return false;
    }
    *out = val;
    return true;
}
