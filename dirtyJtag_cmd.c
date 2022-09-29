/*
  Copyright (c) 2017 Jean THOMAS.
  
  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software
  is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <pico/stdlib.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>

#include "jtag.pio.h"
#include "tusb.h"
#include "pio_jtag.h"
#include "dirtyJtag_cmd.h"


enum CommandIdentifier {
  CMD_STOP = 0x00,
  CMD_INFO = 0x01,
  CMD_FREQ = 0x02,
  CMD_XFER = 0x03,
  CMD_SETSIG = 0x04,
  CMD_GETSIG = 0x05,
  CMD_CLK = 0x06,
  CMD_SETVOLTAGE = 0x07,
  CMD_GOTOBOOTLOADER = 0x08
};

enum CommandModifier
{
  // CMD_XFER
  NO_READ = 0x80,
  EXTEND_LENGTH = 0x40,
  // CMD_CLK
  READOUT = 0x80,
};

enum SignalIdentifier {
  SIG_TCK = 1 << 1,
  SIG_TDI = 1 << 2,
  SIG_TDO = 1 << 3,
  SIG_TMS = 1 << 4,
  SIG_TRST = 1 << 5,
  SIG_SRST = 1 << 6
};

/**
 * @brief Handle CMD_INFO command
 *
 * CMD_INFO returns a string to the host software. This
 * could be used to check DirtyJTAG firmware version
 * or supported commands. As of now it is implemented
 * but not usefull.
 *
 * @param usbd_dev USB device
 */
static void cmd_info();

/**
 * @brief Handle CMD_FREQ command
 *
 * CMD_FREQ sets the clock frequency on the probe.
 * Currently this does not changes anything.
 *
 * @param commands Command data
 */
static void cmd_freq(pio_jtag_inst_t* jtag, const uint8_t *commands);

/**
 * @brief Handle CMD_XFER command
 *
 * CMD_XFER reads and writes data simultaneously.
 *
 * @param usbd_dev USB device
 * @param commands Command data
 */
static void cmd_xfer(pio_jtag_inst_t* jtag, const uint8_t *commands, bool extend_length, bool no_read, uint8_t* tx_buf);

/**
 * @brief Handle CMD_SETSIG command
 *
 * CMD_SETSIG set the logic state of the JTAG signals.
 *
 * @param commands Command data
 */
static void cmd_setsig(pio_jtag_inst_t* jtag, const uint8_t *commands);

/**
 * @brief Handle CMD_GETSIG command
 *
 * CMD_GETSIG gets the current signal state.
 * 
 * @param usbd_dev USB device
 */
static void cmd_getsig(pio_jtag_inst_t* jtag);

/**
 * @brief Handle CMD_CLK command
 *
 * CMD_CLK sends clock pulses with specific TMS and TDI state.
 *
 * @param usbd_dev USB device
 * @param commands Command data
 * @param readout Enable TDO readout
 */
static void cmd_clk(pio_jtag_inst_t *jtag, const uint8_t *commands, bool readout);
/**
 * @brief Handle CMD_SETVOLTAGE command
 *
 * CMD_SETVOLTAGE sets the I/O voltage for devices that support this feature.
 *
 * @param commands Command data
 */
static void cmd_setvoltage(const uint8_t *commands);

/**
 * @brief Handle CMD_GOTOBOOTLOADER command
 *
 * CMD_GOTOBOOTLOADER resets the MCU and enters its bootloader (if installed)
 */
static void cmd_gotobootloader(void);

void cmd_handle(pio_jtag_inst_t* jtag, uint8_t* rxbuf, uint32_t count, uint8_t* tx_buf) {
  uint8_t *commands= (uint8_t*)rxbuf;
  
  while (*commands != CMD_STOP) {
    switch ((*commands)&0x0F) {
    case CMD_INFO:
      cmd_info();
      break;
      
    case CMD_FREQ:
      cmd_freq(jtag, commands);
      commands += 2;
      break;

    case CMD_XFER:
    case CMD_XFER|NO_READ:
    case CMD_XFER|EXTEND_LENGTH:
    case CMD_XFER|NO_READ|EXTEND_LENGTH:
      cmd_xfer(jtag, commands, *commands & EXTEND_LENGTH, *commands & NO_READ, tx_buf);
      return;
      break;

    case CMD_SETSIG:
      cmd_setsig(jtag, commands);
      commands += 2;
      break;

    case CMD_GETSIG:
      cmd_getsig(jtag);
      break;

    case CMD_CLK:
      cmd_clk(jtag, commands, !!(*commands & READOUT));
      commands += 2;
      break;

    case CMD_SETVOLTAGE:
      cmd_setvoltage(commands);
      commands += 1;
      break;

    case CMD_GOTOBOOTLOADER:
      cmd_gotobootloader();
      break;

    default:
      return; /* Unsupported command, halt */
      break;
    }

    commands++;
  }

  return;
}

static void cmd_info() {
  char info_string[10] = "DJTAG2\n";

  tud_vendor_write((uint8_t*)info_string, 10);
}

static void cmd_freq(pio_jtag_inst_t* jtag, const uint8_t *commands) {
  jtag->freq_khz = (commands[1] << 8) | commands[2];
  jtag_set_clk_freq(jtag);
}

//static uint8_t output_buffer[64];

static void cmd_xfer(pio_jtag_inst_t* jtag, const uint8_t *commands, bool extend_length, bool no_read, uint8_t* tx_buf) {
  uint16_t transferred_bits;
  uint8_t* output_buffer = 0;
  
  /* Fill the output buffer with zeroes */
  if (!no_read) {
    output_buffer = tx_buf;
    memset(output_buffer, 0, 64);
  }

  /* This is the number of transfered bits in one transfer command */
  transferred_bits = commands[1];
  if (extend_length) {
    transferred_bits += 256;
    // Ensure we don't do over-read
    if (transferred_bits > 62*8) {
      return;
    }
  }
  jtag_transfer(jtag, transferred_bits, commands+2, output_buffer);

  /* Send the transfer response back to host */
  if (!no_read) {
    tud_vendor_write(output_buffer, 32);
    //tud_vendor_write(output_buffer, (transferred_bits + 7)/8);
  }
}

static void cmd_setsig(pio_jtag_inst_t* jtag, const uint8_t *commands) {
  uint8_t signal_mask, signal_status;

  signal_mask = commands[1];
  signal_status = commands[2];

  if (signal_mask & SIG_TCK) {
    jtag_set_clk(jtag,signal_status & SIG_TCK);
  }

  if (signal_mask & SIG_TDI) {
    jtag_set_tdi(jtag, signal_status & SIG_TDI);
  }

  if (signal_mask & SIG_TMS) {
    jtag_set_tms(jtag, signal_status & SIG_TMS);
  }
  
  if ((signal_mask & SIG_TRST) && jtag->trst.available) {
    jtag_set_trst(jtag, signal_status & SIG_TRST);
  }

  if ((signal_mask & SIG_SRST) && jtag->srst.available) {
    jtag_set_srst(jtag, signal_status & SIG_SRST);
  }
}

static void cmd_getsig(pio_jtag_inst_t* jtag) {
  uint8_t signal_status = 0;
  
  if (jtag_get_tdo(jtag)) {
    signal_status |= SIG_TDO;
  }
  tud_vendor_write(&signal_status, 1);
}

static void cmd_clk(pio_jtag_inst_t *jtag, const uint8_t *commands, bool readout)
{
  uint8_t signals, clk_pulses;
  signals = commands[1];
  clk_pulses = commands[2];
  uint8_t readout_val = jtag_strobe(jtag, clk_pulses, signals & SIG_TMS, signals & SIG_TDI);

  if (readout)
  {
    tud_vendor_write(&readout_val, 1);
  }
  
}

static void cmd_setvoltage(const uint8_t *commands) {
  (void)commands;
}

static void cmd_gotobootloader(void) {

}