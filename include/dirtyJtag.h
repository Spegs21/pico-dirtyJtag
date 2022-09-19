#ifndef _DIRTYJTAG_H
#define _DIRTYJTAG_H

#define MULTICORE
#define n_buffers (4)

typedef struct
{
    volatile uint8_t count;
    volatile uint8_t busy;
    cmd_buffer buffer;
} buffer_info;

void djtag_init(PIO pio, uint8_t sm, uint16_t jtag_freq_khz,
                uint8_t tdi_pin, uint8_t tdo_pin, uint8_t tck_pin,
                uint8_t tms_pin, uint8_t rst_pin, uint8_t trst_pin);
void djtag_task();

#endif