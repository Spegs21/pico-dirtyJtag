#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "tusb.h"
#include "dirtyJtag.h"
#include "pio_jtag.h"
#include "dirtyJtag_cmd.h"

static pio_jtag_inst_t pio_jtag_inst;
static uint wr_buffer_number = 0;
static uint rd_buffer_number = 0; 
static uint8_t tx_buf[64];
buffer_info buffer_infos[n_buffers];

static void jtag_main_task()
{
#ifdef MULTICORE
    if (multicore_fifo_rvalid())
    {
        //some command processing has been done
        uint rx_num = multicore_fifo_pop_blocking();
        buffer_info* bi = &buffer_infos[rx_num];
        bi->busy = false;

    }
#endif
    if ((buffer_infos[wr_buffer_number].busy == false)) 
    {
        if (tud_vendor_available())
        {
            uint bnum = wr_buffer_number;
            uint count = tud_vendor_read(buffer_infos[wr_buffer_number].buffer, 64);
            if (count != 0)
            {
                buffer_infos[bnum].count = count;
                buffer_infos[bnum].busy = true;
                wr_buffer_number = wr_buffer_number + 1; //switch buffer
                if (wr_buffer_number == n_buffers)
                {
                    wr_buffer_number = 0; 
                }
#ifdef MULTICORE
                multicore_fifo_push_blocking(bnum);
#endif
            }
        }

    }       
}

#ifdef MULTICORE
static void core1_entry() {

    init_jtag(&pio_jtag_inst);
    while (1)
    {
        uint rx_num = multicore_fifo_pop_blocking();
        buffer_info* bi = &buffer_infos[rx_num];
        assert (bi->busy);
        cmd_handle(&pio_jtag_inst, bi->buffer, bi->count, tx_buf);
        multicore_fifo_push_blocking(rx_num);
    }
 
}
#endif

static void fetch_command()
{
#ifndef MULTICORE
    if (buffer_infos[rd_buffer_number].busy)
    {
        cmd_handle(&pio_jtag_inst, buffer_infos[rd_buffer_number].buffer, buffer_infos[rd_buffer_number].count, tx_buf);
        buffer_infos[rd_buffer_number].busy = false;
        rd_buffer_number++; //switch buffer
        if (rd_buffer_number == n_buffers)
        {
            rd_buffer_number = 0; 
        }
    }
#endif
}

//this is to work around the fact that tinyUSB does not handle setup request automatically
//Hence this boiler plate code
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
    if (stage != CONTROL_STAGE_SETUP) return true;
    return false;
}

void jtag_task()
{
#ifndef MULTICORE
    jtag_main_task();
#endif
}

void djtag_trst_init(uint8_t trst_pin, djtag_rst_active_t act)
{
    init_rst(&pio_jtag_inst, PIO_JTAG_TRST, trst_pin, act);

}

void djtag_srst_init(uint8_t srst_pin, djtag_rst_active_t act)
{
    init_rst(&pio_jtag_inst, PIO_JTAG_SRST, srst_pin, act);
}

void djtag_init(PIO pio, uint8_t sm,
                uint8_t tdi_pin, uint8_t tdo_pin, uint8_t tck_pin,
                uint8_t tms_pin)
{
    pio_jtag_inst.freq_khz = 1000;
    pio_jtag_inst.pio = pio;
    pio_jtag_inst.sm = sm;
    pio_jtag_inst.pins.tdi = tdi_pin;
    pio_jtag_inst.pins.tdo = tdo_pin;
    pio_jtag_inst.pins.tck = tck_pin;
    pio_jtag_inst.pins.tms = tms_pin;
    pio_jtag_inst.srst.available = false;
    pio_jtag_inst.trst.available = false;
    
    #ifdef MULTICORE
    multicore_launch_core1(core1_entry);
    #else 
    init_jtag(&pio_jtag_inst);
    #endif
}

void djtag_task()
{
    jtag_main_task();
    fetch_command();//for unicore implementation
}
