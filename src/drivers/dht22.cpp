#include "drv_dht22.h"
#include "../config/pins.h"

static float s_temp    = 0.0f;
static float s_hum     = 0.0f;
static bool  s_last_ok = false;

static inline uint16_t timer1_ticks()
{
    return TCNT1;   // raw ticks at 2MHz (0.5µs each), wraps correctly at uint16_t boundary
}

// Wait for pin to reach `level`, return false if it takes longer than timeout_ticks.
static bool wait_for(volatile uint8_t *pin_reg, uint8_t mask, uint8_t level, uint16_t timeout_ticks)
{
    uint16_t start = timer1_ticks();
    //while (digitalRead(pin) != level)
    while (((*pin_reg & mask) ? 1 : 0) != level)
    {
        if ((uint16_t)(timer1_ticks() - start) >= timeout_ticks)
            return false;
    }
    return true;
}

// Measure how long pin stays at `level` (ticks). Returns 0 on timeout.
static uint16_t measure_pulse(volatile uint8_t *pin_reg, uint8_t mask, uint8_t level, uint16_t timeout_ticks)
{
    uint16_t start = timer1_ticks();
    //while (digitalRead(pin) == level)
    while (((*pin_reg & mask) ? 1 : 0) == level)
    {
        if ((uint16_t)(timer1_ticks() - start) >= timeout_ticks)
            return 0;
    }
    return (uint16_t)(timer1_ticks() - start);
}

static bool read_sensor(float *temp, float *hum)
{
    uint8_t data[5] = {0};
    bool ok = false;

    taskENTER_CRITICAL();

    // Send start signal: pull low for 1 ms, then release.
    //pinMode(PIN_DHT22, OUTPUT);
    DHT22_DDR  |=  (1 << DHT22_BIT);
    //digitalWrite(PIN_DHT22, LOW);
    DHT22_PORT &= ~(1 << DHT22_BIT);
    //delay(1);
    _delay_ms(1);
    //pinMode(PIN_DHT22, INPUT_PULLUP);
    DHT22_DDR  &= ~(1 << DHT22_BIT);   // clear bit → INPUT
    DHT22_PORT |=  (1 << DHT22_BIT);   // set bit in PORT → enables internal pull-up

    // Sensor responds with 80 µs LOW then 80 µs HIGH before sending data.
    if (!wait_for(&DHT22_PINR, (1 << DHT22_BIT), LOW,  200))
        goto done; // wait for sensor to pull LOW
    if (!wait_for(&DHT22_PINR, (1 << DHT22_BIT), HIGH, 300))
        goto done; // wait for LOW to end (~80 µs)
    if (!wait_for(&DHT22_PINR, (1 << DHT22_BIT), LOW,  300))
        goto done; // wait for HIGH to end (~80 µs)

    // Read 40 bits. Each bit: 50 µs LOW start pulse, then HIGH whose duration
    // encodes the value (< 50 µs = 0, > 50 µs = 1).
    for (uint8_t i = 0; i < 40; i++)
    {
        if (!wait_for(&DHT22_PINR, (1 << DHT22_BIT), HIGH, 200))
            goto done; // wait for 50 µs LOW to end
        uint16_t high_ticks = measure_pulse(&DHT22_PINR, (1 << DHT22_BIT), HIGH, 300);
        if (high_ticks == 0)
            goto done;

        data[i / 8] <<= 1;
        if (high_ticks > 100)   // 100 ticks = 50 µs threshold
            data[i / 8] |= 1;
    }

    // Checksum: last byte must equal lower 8 bits of sum of first four.
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
        goto done;

    *hum  = ((data[0] << 8) | data[1]) * 0.1f;
    *temp = (((data[2] & 0x7F) << 8) | data[3]) * 0.1f;
    if (data[2] & 0x80)
        *temp = -*temp;
    ok = true;

done:
    taskEXIT_CRITICAL();
    return ok;
}

bool dht22_init()
{
    // Timer1: free-running, prescaler 8 → 2MHz → 0.5µs per tick
    TCCR1A = 0;
    TCCR1B = (1 << CS11);

    //pinMode(PIN_DHT22, INPUT_PULLUP);
    DHT22_DDR  &= ~(1 << DHT22_BIT);
    DHT22_PORT |=  (1 << DHT22_BIT);
    return true;
}

bool dht22_read_temperature(float *out)
{
    s_last_ok = read_sensor(&s_temp, &s_hum);
    if (!s_last_ok)
        return false;
    *out = s_temp;
    return true;
}

bool dht22_read_humidity(float *out)
{
    if (!s_last_ok)
        return false;
    *out = s_hum;
    return true;
}
