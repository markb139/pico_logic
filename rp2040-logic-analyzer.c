/**
 * Modified by Mark Burton 2021
 * Removed serial command handling
 * use dma for background capture
 * use 2 different PIO programms for fast and slow capture - min PIO clock is 2khz
 * /

/**
 * Modified by Mark Komus 2021
 * Now repeatedly captures data and outputs to a CSV format
 * Intended to be imported by sigrok / PulseView
 *
 */

/**
 *
 * Original code (found in the pico examples project):
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "usbtmc.pio.h"

uint offset;
struct pio_program *capture_prog_2=NULL;

void load_slow_capture(PIO pio)
{
    if(capture_prog_2)
        pio_remove_program(pio, capture_prog_2, offset);

    capture_prog_2 = &slow_in_program;

    offset = pio_add_program(pio, &slow_in_program);
}

void load_fast_capture(PIO pio, uint pin_count)
{
    if(capture_prog_2)
        pio_remove_program(pio, capture_prog_2, offset);

    capture_prog_2 = &fast_in_program;

    offset = pio_add_program(pio, &fast_in_program);
}

void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, float div) {
    // Load a program to capture n pins. This is just a single `in pins, n`
    // instruction with a wrap.
    
    pio_sm_config c = pio_get_default_sm_config();

    if( div < 62500.0)  
    {
        load_fast_capture(pio, 8);
        sm_config_set_wrap(&c, offset, offset);
    }
    else
    {
        div = div / 2000;
        load_slow_capture(pio);
        sm_config_set_wrap(&c, offset, offset+7);
    }


    // Configure state machine to loop over this `in` instruction forever,
    // with autopush enabled.
    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_out_pins(&c, 25,1);

    sm_config_set_clkdiv(&c, div);
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, sm, offset, &c);
}

void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words,
                        uint trigger_pin, bool trigger_level, irq_handler_t dma_handler) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destinatinon pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        true                // Start immediately
    );
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));
    pio_sm_set_enabled(pio, sm, true);
}
