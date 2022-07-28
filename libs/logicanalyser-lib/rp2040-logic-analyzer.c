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


uint compile_capture(PIO pio, pio_sm_config *c, uint pin_count, uint trigger_pin, uint trigger_type, float div);

uint offset;
struct pio_program *current_program=NULL;
uint16_t program_instructions[32];
struct pio_program pio_program;

/*******************************************************************************************
 * Initialise the logic analyser program
 * 
 * There are two PIO programs that can be loaded depending on the clock divisor.
 * If the sampling clock is below 2KHz then a slow version is loaded and the clock sped up.
 * Otherwise a fast program is loaded with samples the pins as fast as the clock is going - up to 125MHz
 * 
 * The programs support triggering be level and edge on one GPIO pin
 * 
 * *****************************************************************************************/
void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, uint trigger_pin, uint trigger_type, float div) 
{
    uint prog_offset;
    uint wrap_target;
    uint wrap;

    pio_sm_config c = pio_get_default_sm_config();
    // remove any current PIO proram from the statemachine
    if(current_program)
        pio_remove_program(pio, current_program, offset);

    // compile and load PIO capture program
    offset = compile_capture(pio, &c, pin_count, trigger_pin, trigger_type, div);

    // configure statemachine IN pins
    sm_config_set_in_pins(&c, pin_base);
    
    // configure fifos
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    // initialise the statemachine so that it's ready to run
    pio_sm_init(pio, sm, offset, &c);
}

/*******************************************************************************************
 * Arm the logic analyser for a capture run
 * 
 * This configures the DMA so that captured data is stored without loading the CPU core
 * The state machine is enabled and thus starts at the end. If the program has a trigger enabled
 * then it will stall until the condition is met.
 * 
 * *****************************************************************************************/
void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words, irq_handler_t dma_handler) 
{
    // stop the statemachine and clear down fifos.
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);

    // configure the DMA so that it reads data from the statemachine
    // the data rate is controlled by the statemachine DREQ
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

    // generate an IRQ and the end of the capture
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // configure a one shot DMA request.
    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destinatinon pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        true                // Start immediately
    );

    // start the statemachine with the capture PIO program loaded
    pio_sm_set_enabled(pio, sm, true);
}

uint8_t add_level_trigger(bool level, uint trigger_pin, uint16_t* program, uint8_t prog_offset)
{
    program[prog_offset] = pio_encode_wait_gpio(level, trigger_pin);
    return 1;
}

uint8_t add_edge_trigger(bool edge, uint trigger_pin, uint16_t* program, uint8_t prog_offset)
{
    if(edge)
    {
        program[prog_offset] = pio_encode_wait_gpio(false, trigger_pin)  | pio_encode_sideset(1,1);
        program[prog_offset+1] = pio_encode_wait_gpio(true, trigger_pin);
    }
    else
    {
        program[prog_offset] = pio_encode_wait_gpio(true, trigger_pin) | pio_encode_sideset(1,1);
        program[prog_offset+1] = pio_encode_wait_gpio(false, trigger_pin);
    }
    
    return 2;
}


uint compile_slow_capture(PIO pio, uint pin_count, uint trigger_pin, uint trigger_type)
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

    program_instructions[prog_offset++] = pio_encode_in(pio_pins, pin_count) | pio_encode_sideset(1,0);
    program_instructions[prog_offset++] = pio_encode_set(pio_x, 19);
    uint jmp_inst = prog_offset;
    program_instructions[prog_offset++] = pio_encode_nop() | pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_nop() | pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_nop() | pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_nop() | pio_encode_delay(20);
    program_instructions[prog_offset++] = pio_encode_nop() | pio_encode_delay(20);
    program_instructions[prog_offset++] = _pio_encode_instr_and_args(pio_instr_bits_jmp, 2, jmp_inst);

    return prog_offset;
}

uint compile_fast_capture(PIO pio, uint pin_count, uint trigger_pin, uint trigger_type)
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

    program_instructions[prog_offset++] =  pio_encode_in(pio_pins,pin_count)  | pio_encode_sideset(1,0);

    return prog_offset;
}

uint load_program(PIO pio, uint prog_offset, struct pio_program *pio_program, struct pio_program **current_program)
{
    pio_program->instructions = program_instructions;
    pio_program->length = prog_offset;
    pio_program->origin = -1;
    
    *current_program = pio_program;

    return pio_add_program(pio, *current_program);
}

uint compile_capture(PIO pio, pio_sm_config *c, uint pin_count, uint trigger_pin, uint trigger_type, float div)
{
    uint prog_offset;
    uint wrap_target;
    uint wrap;
    uint load_offset;

    if( div < 62500.0)  
    {
        wrap_target = 0;
        wrap = 0;
        if(trigger_type == 1 || trigger_type == 2)
        {
            wrap_target = 1;
            wrap = 1;
        }
        else if(trigger_type == 3 || trigger_type == 4)
        {
            wrap_target = 2;
            wrap = 2;
        }
        prog_offset = compile_fast_capture(pio, pin_count, trigger_pin, trigger_type);
    }
    else
    {
        div = div / 2000;

        wrap_target = 0;
        wrap = 7;
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
        prog_offset = compile_slow_capture(pio, pin_count, trigger_pin, trigger_type);
    }
    load_offset = load_program(pio, prog_offset, &pio_program, &current_program);
    sm_config_set_wrap(c, load_offset + wrap_target, load_offset + wrap);
    sm_config_set_clkdiv(c, div);
 
    return load_offset;
}
