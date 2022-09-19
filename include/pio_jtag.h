
#ifndef _PIO_JTAG_H
#define _PIO_JTAG_H

#include "hardware/pio.h"

#define DMA

typedef struct 
{
    PIO pio;
    uint8_t sm;
    uint16_t freq_khz;
    struct pins
    {
        uint8_t tdi;
        uint8_t tdo;
        uint8_t tck;
        uint8_t tms;
        uint8_t rst;
        uint8_t trst;
    }
} pio_jtag_inst_t;

void init_jtag(pio_jtag_inst_t* jtag);

void pio_jtag_write_blocking(const pio_jtag_inst_t *jtag, const uint8_t *src, size_t len);

void pio_jtag_write_read_blocking(const pio_jtag_inst_t *jtag, const uint8_t *src, uint8_t *dst, size_t len);

uint8_t pio_jtag_write_tms_blocking(const pio_jtag_inst_t *jtag, bool tdi, bool tms, size_t len);

void jtag_set_clk_freq(const pio_jtag_inst_t *jtag);

void jtag_transfer(const pio_jtag_inst_t *jtag, uint32_t length, const uint8_t* in, uint8_t* out);

uint8_t jtag_strobe(const pio_jtag_inst_t *jtag, uint32_t length, bool tms, bool tdi);


static inline void jtag_set_tms(const pio_jtag_inst_t *jtag, bool value)
{
    gpio_put(jtag->pin_tms, value);
}
static inline void jtag_set_rst(const pio_jtag_inst_t *jtag, bool value)
{
    gpio_put(jtag->pin_rst, value);
}
static inline void jtag_set_trst(const pio_jtag_inst_t *jtag, bool value)
{
    gpio_put(jtag->pin_trst, value);
}

// The following APIs assume that they are called in the following order:
// jtag_set_XXX where XXX is any pin. if XXX is clk it needs to be false
// jtag_set_clk where clk is true This will initiate a one cycle pio_jtag_write_read_blocking
// possibly jtag_get_tdo which will get what was read during the previous step
void jtag_set_tdi(const pio_jtag_inst_t *jtag, bool value);

void jtag_set_clk(const pio_jtag_inst_t *jtag, bool value);

bool jtag_get_tdo(const pio_jtag_inst_t *jtag);



#endif