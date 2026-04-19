#include "drv_dht22.h"
#include "../config/pins.h"

#include <DHTNEW.h>

static DHTNEW dht(PIN_DHT22);
static bool   last_read_ok = false;

bool dht22_init() {
    // Do not disable interrupts — feilipu/FreeRTOS WDT resets after 30 ms of cli().
    dht.setDisableIRQ(false);
    return true;
}

bool dht22_read_temperature(float *out) {
    last_read_ok = (dht.read() == DHTLIB_OK);
    if (!last_read_ok) return false;
    *out = dht.getTemperature();
    return true;
}

bool dht22_read_humidity(float *out) {
    if (!last_read_ok) return false;
    *out = dht.getHumidity();
    return true;
}
