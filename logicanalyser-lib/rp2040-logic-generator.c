/*****
 * 
 */

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "logic_analyser.pio.h"


#define GEN_DMA_CHAN 1

typedef struct {
    PIO pio;
    uint state_machine;
    uint generator_offset;
    struct pio_program const *generator_current_program;
    uint32_t random_buf[32];
    bool dma_conf;
    bool random_run;
    uint pin_base;
    uint pin_count;
    dma_channel_config dma_c;
} Generator;

Generator* generator=NULL;

void generator_initialise(PIO pio, uint sm)
{
    generator = (Generator*)malloc(sizeof(Generator));
    generator->generator_offset = 0;
    generator->generator_current_program = NULL;
    generator->dma_conf = false;
    generator->random_run = false;
    generator->pio = pio;
    generator->state_machine = sm;
    generator->pin_base = 10;
    generator->pin_count = 8;
    generator->dma_c = dma_channel_get_default_config(GEN_DMA_CHAN);
    channel_config_set_read_increment(&generator->dma_c, true);
    channel_config_set_write_increment(&generator->dma_c, false);
    channel_config_set_dreq(&generator->dma_c, pio_get_dreq(pio, sm, true));
    channel_config_set_transfer_data_size(&generator->dma_c, DMA_SIZE_32);
}

void generate_random()
{
    for(int i=0;i<32;i++)
    {
        generator->random_buf[i] = random() & 0xffffffff;
    }
    generator->dma_conf = true;

    // configure a one shot DMA request.
    dma_channel_configure(GEN_DMA_CHAN, &generator->dma_c,
        &generator->pio->txf[generator->state_machine],   // Destinatinon pointer
        generator->random_buf,      // Source pointer
        32,              // Number of transfers
        true             // Start immediately
    );
}

void random_handler()
{
    dma_hw->ints1 = 1u << 1;
    
    if(generator->random_run)
        generate_random();
}

void generate_pattern(PIO pio, uint sm, uint pattern, float div)
{
    if(!generator)
        generator_initialise(pio, sm);

    if(generator->generator_current_program)
    {
        pio_sm_set_enabled(pio, sm, false);
        pio_remove_program(pio, generator->generator_current_program, generator->generator_offset);
        generator->generator_current_program = NULL;
        generator->generator_offset = 0;
        if(generator->dma_conf)
        {
            generator->random_run = false;
            dma_channel_abort(1);
        }
    }
    
    if(pattern == 1)
    {
        generator->generator_current_program = &square_wave_program;
        generator->generator_offset = pio_add_program(pio, &square_wave_program);
        pio_sm_config c = square_wave_program_get_default_config(generator->generator_offset);
        sm_config_set_set_pins(&c, generator->pin_base, 1);
        pio_gpio_init(pio, generator->pin_base);
        pio_sm_set_consecutive_pindirs(pio, sm, generator->pin_base, 1, true);

        sm_config_set_clkdiv(&c, div);
        pio_sm_init(pio, sm, generator->generator_offset, &c);
        pio_sm_set_enabled(pio, sm, true);
    }
    else if(pattern == 2)
    {
        for(int i=generator->pin_base;i<generator->pin_base + generator->pin_count;i++)
            pio_gpio_init(pio, i);
        
        pio_sm_set_consecutive_pindirs(pio, sm, generator->pin_base, generator->pin_count, true);

        generator->generator_current_program = &count_program;
        generator->generator_offset = pio_add_program(pio, &count_program);
        pio_sm_config c = count_program_get_default_config(generator->generator_offset);
        
        sm_config_set_out_pins(&c, generator->pin_base, generator->pin_count);
        
        sm_config_set_clkdiv(&c, div);
        pio_sm_init(pio, sm, generator->generator_offset, &c);
        pio_sm_put_blocking(pio, sm, 0xffffffff);
        pio_sm_set_enabled(pio, sm, true);
    }
    else if(pattern == 3)
    {
        generator->generator_current_program = &random_program;
        generator->generator_offset = pio_add_program(pio, &random_program);
        pio_sm_config c = random_program_get_default_config(generator->generator_offset);
        sm_config_set_out_pins(&c, generator->pin_base, generator->pin_count);
        
        for(int i=generator->pin_base;i<generator->pin_base+generator->pin_count; i++)
            pio_gpio_init(pio, i);

        pio_sm_set_consecutive_pindirs(pio, sm, generator->pin_base, generator->pin_count, true);

        sm_config_set_clkdiv(&c, div);
        pio_sm_init(pio, sm, generator->generator_offset, &c);
        pio_sm_set_enabled(pio, sm, true);

        if(!generator->dma_conf)
        {
            // generate an IRQ and the end of the capture
            dma_channel_set_irq1_enabled(GEN_DMA_CHAN, true);
            irq_set_exclusive_handler(DMA_IRQ_1, random_handler);
            irq_set_enabled(DMA_IRQ_1, true);
        }
        generator->random_run = true;
        generate_random();
    }
    else
    {}
}
