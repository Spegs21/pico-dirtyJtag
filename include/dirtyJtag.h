#ifndef _DIRTYJTAG_H
#define _DIRTYJTAG_H

#include "hardware/pio.h"

#define MULTICORE
#define n_buffers (4)

typedef enum
{
    DJTAG_RST_ACTIVE_LOW,
    DJTAG_RST_ACTIVE_HIGH
} djtag_rst_active_t;

typedef struct
{
    volatile uint8_t count;
    volatile uint8_t busy;
    uint8_t buffer[64];
} buffer_info;

void djtag_init(PIO pio, uint8_t sm, uint8_t tdi_pin,
                uint8_t tdo_pin, uint8_t tck_pin, uint8_t tms_pin);
void djtag_srst_init(uint8_t srst_pin, djtag_rst_active_t act);
void djtag_trst_init(uint8_t trst_pin, djtag_rst_active_t act);
void djtag_task();

#endif