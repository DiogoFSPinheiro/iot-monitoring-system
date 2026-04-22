#include "drv_bh1750.h"

// Spin until TWINT is set, or timeout. Returns false on timeout.
static bool twi_wait()
{
    uint16_t count = TWI_TIMEOUT_ITERS;
    while (!(TWCR & (1 << TWINT)))
    {
        if (--count == 0)
            return false;
    }
    return true;
}

static bool twi_start()
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!twi_wait())
        return false;
    uint8_t status = TWSR & 0xF8;
    return (status == 0x08 || status == 0x10);  // START or repeated START
}

static void twi_stop()
{
    // TWSTO is cleared automatically by hardware after STOP is generated.
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

// Send SLA+W and expect ACK (status 0x18), or send data byte and expect ACK (0x28).
static bool twi_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait())
        return false;
    uint8_t status = TWSR & 0xF8;
    return (status == 0x18 || status == 0x28);
}

// Send SLA+R after START and expect ACK. Status 0x40 = MR mode address ACK.
// This is distinct from twi_write — the hardware uses a different status code
// for a read-direction address byte than for a write-direction one.
static bool twi_addr_r()
{
    TWDR = (BH1750_ADDR << 1) | 1;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait())
        return false;
    return (TWSR & 0xF8) == 0x40;
}

// Read one byte and send ACK (more bytes to follow). Status 0x50.
static bool twi_read_ack(uint8_t *out)
{
    TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
    if (!twi_wait())
        return false;
    if ((TWSR & 0xF8) != 0x50)
        return false;
    *out = TWDR;
    return true;
}

// Read one byte and send NACK (last byte). Status 0x58.
static bool twi_read_nack(uint8_t *out)
{
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!twi_wait())
        return false;
    if ((TWSR & 0xF8) != 0x58)
        return false;
    *out = TWDR;
    return true;
}

bool bh1750_init()
{
    // 100kHz: TWBR = (F_CPU/SCL - 16) / (2 * prescaler) = (160 - 16) / 2 = 72
    TWSR &= ~0x03;  // prescaler = 1
    TWBR = 72;

    if (!twi_start())
        return false;
    if (!twi_write((BH1750_ADDR << 1) | 0)) {
        twi_stop();
        return false;
    }
    if (!twi_write(BH1750_POWER_ON)) {
        twi_stop();
        return false;
    }
    twi_stop();

    _delay_ms(10);

    if (!twi_start())
        return false;
    if (!twi_write((BH1750_ADDR << 1) | 0)) {
        twi_stop();
        return false;
    }
    if (!twi_write(BH1750_CONT_HR)) {
        twi_stop();
        return false;
    }
    twi_stop();

    // Wait for first measurement to be ready (120ms typ, 180ms with margin).
    _delay_ms(180);

    return true;
}

bool bh1750_read(float *out)
{
    uint8_t msb, lsb;

    if (!twi_start())
        return false;
    if (!twi_addr_r()) {
        twi_stop();
        return false;
    }
    if (!twi_read_ack(&msb)) {
        twi_stop();
        return false;
    }
    if (!twi_read_nack(&lsb)) {
        twi_stop();
        return false;
    }
    twi_stop();

    uint16_t raw = ((uint16_t)msb << 8) | lsb;
    *out = raw / 1.2f;
    return true;
}
