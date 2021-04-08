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
struct pio_program *current_program=NULL;

uint8_t add_level_trigger(bool level, uint trigger_pin, uint16_t* program, uint8_t prog_offset)
{
    program[prog_offset] = pio_encode_wait_gpio(level, trigger_pin);
    return 1;
}

uint8_t add_edge_trigger(bool edge, uint trigger_pin, uint16_t* program, uint8_t prog_offset)
{
    if(edge)
    {
        program[prog_offset] = pio_encode_wait_gpio(false, trigger_pin);
        program[prog_offset+1] = pio_encode_wait_gpio(true, trigger_pin);
    }
    else
    {
        program[prog_offset] = pio_encode_wait_gpio(true, trigger_pin);
        program[prog_offset+1] = pio_encode_wait_gpio(false, trigger_pin);
    }
    
    return 2;
}

uint16_t program_instructions[32];
struct pio_program slow_in_program2;

uint load_slow_capture(PIO pio, uint pin_count, uint trigger_pin, uint trigger_type)
{
    uint8_t prog_offset = 0;
    if(trigger_type == 1 || trigger_type == 2)
    {
        bool hi_level = trigger_type == 2 ? true: false;
        prog_offset = add_level_trigger(hi_level, trigger_pin, program_instructions, 0);
    }
    else if(trigger_type == 3 || trigger_type == 4)
    {
        bool hi_level = trigger_type == 3 ? true: false;
        prog_offset = add_edge_trigger(hi_level, trigger_pin, program_instructions, 0);
    }

    program_instructions[prog_offset++] = pio_encode_in(pio_pins, pin_count);
    program_instructions[prog_offset++] = pio_encode_set(pio_x, 19);
    uint jmp_inst = prog_offset;
    program_instructions[prog_offset++] = pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_delay(20);
    program_instructions[prog_offset++] = _pio_encode_instr_and_args(pio_instr_bits_jmp, 2, jmp_inst);

    slow_in_program2.instructions = program_instructions;
    slow_in_program2.length = prog_offset;
    slow_in_program2.origin = -1;
    
    current_program = &slow_in_program2;

    return pio_add_program(pio, current_program);
}

uint load_fast_capture(PIO pio, uint pin_count, uint trigger_pin, uint trigger_type)
{
    uint16_t program_instructions2[32];
    uint8_t prog_offset = 0;
    if(trigger_type == 1 || trigger_type == 2)
    {
        bool hi_level = trigger_type == 2 ? true: false;
        prog_offset = add_level_trigger(hi_level, trigger_pin, program_instructions2, 0);
    }
    else if(trigger_type == 3 || trigger_type == 4)
    {
        bool hi_level = trigger_type == 3 ? true: false;
        prog_offset = add_edge_trigger(hi_level, trigger_pin, program_instructions2, 0);
    }

    program_instructions2[prog_offset] =  pio_encode_in(pio_pins,pin_count);

    struct pio_program fast_in_program2 = {
        .instructions = program_instructions2,
        .length = 1 + prog_offset,
        .origin = -1,
    };

    current_program = &fast_in_program2;
    return pio_add_program(pio, current_program);
}

void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, uint trigger_pin, uint trigger_type, float div) {
    // Load a program to capture n pins. This is just a single `in pins, n`
    // instruction with a wrap.
    
    pio_sm_config c = pio_get_default_sm_config();
    if(current_program)
        pio_remove_program(pio, current_program, offset);


    if( div < 62500.0)  
    {
        uint fast_in_wrap_target = 0;
        uint fast_in_wrap = 0;
        if(trigger_type == 1 || trigger_type == 2)
        {
            fast_in_wrap_target = 1;
            fast_in_wrap = 1;
        }
        else if(trigger_type == 3 || trigger_type == 4)
        {
            fast_in_wrap_target = 2;
            fast_in_wrap = 2;
        }
        offset = load_fast_capture(pio, pin_count, trigger_pin, trigger_type);
        sm_config_set_wrap(&c, offset + fast_in_wrap_target, offset + fast_in_wrap );
    }
    else
    {
        div = div / 2000;
        offset = load_slow_capture(pio, pin_count, trigger_pin, trigger_type);
        uint wrap_target = 0;
        uint wrap = 7;
        if(trigger_type == 1 || trigger_type == 2)
        {
            wrap_target = 1;
            wrap = 8;
        }
        else if(trigger_type == 3 || trigger_type == 4)
        {
            wrap_target = 2;
            wrap = 9;
        }

        sm_config_set_wrap(&c, offset + wrap_target, offset + wrap);
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

void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words, irq_handler_t dma_handler) 
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destinatinon pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        true                // Start immediately
    );
    pio_sm_set_enabled(pio, sm, true);
}
