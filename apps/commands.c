#include <stdio.h>
#include <string.h>
#include <stdlib.h>   
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "logic_analyser.h"
#include "main.h"
#include "commands.h"

static inline uint32_t tu_max32 (uint32_t x, uint32_t y) { return (x > y) ? x : y; }
void dma_irq();
uint dma_chan;

#define _CMD(_CMD_STR, _STR_LEN, _FUNC) \
    if(aLen >=_STR_LEN && !strncasecmp(_CMD_STR, (char*)aData,_STR_LEN)) \
        _FUNC(aData, aLen);

bool process_command(uint8_t* aData, size_t aLen)
{
    _CMD("*idn?", 4, process_idn);
    _CMD("*opc?", 4, process_opc);
    _CMD("*esr?", 4, process_esr);
    _CMD("l:capture", 9, process_capture);
    _CMD("l:pat", 5, process_pattern);
    _CMD("rate", 4, process_rate);
    _CMD("trig", 4, process_trigger);
    _CMD("data?", 5, process_data);

    return true;
}

void process_idn(uint8_t const *aBuffer, size_t aLen)
{
    command_complete(idn, strlen((const char*)idn));
}

void process_pattern(uint8_t const *aBuffer, size_t aLen)
{
    pattern = atof((char*) aBuffer + 6);
}

void process_rate(uint8_t const *aBuffer, size_t aLen)
{
    sample_rate = atof((char*) aBuffer + 5);
}

void process_trigger(uint8_t const *aBuffer, size_t aLen)
{
    trig_channel = atof((char*) aBuffer + 5);
    trig_type = atof((char*) aBuffer + 7);
}

void process_data(uint8_t const *aBuffer, size_t aLen)
{
        process_capture_result();
}

void process_opc(uint8_t const *aBuffer, size_t aLen)
{
    if(commandComplete)
        command_complete(opc_1, strlen((const char*)opc_1));
    else
        command_complete(opc_0, strlen((const char*)opc_0));
}

void process_esr(uint8_t const *aBuffer, size_t aLen)
{
    if(esr_buf)
    {
        free(esr_buf);
    }
    
    esr_buf = malloc(32);

    sprintf((char*)esr_buf, "%ld\r\n", status_register);
    command_complete((const uint8_t *)esr_buf, strlen((const char *)esr_buf));
    status_register = 0;
}

void process_capture(uint8_t const *aData, size_t aLen)
{
    PIO pio = pio0;
    uint sm = 0;
    uint pin_base = ANALYSER_PIN_BASE;

    num_samples = tu_max32(atoi((char*)aData + 10), 1);
    if(num_samples > 200000)
    {
        commandComplete = true;
        sampleRun = false;
        status_register |= 0x00000001;
    }
    else
    {
        float sample_div = (float) clock_get_hz(clk_sys) / sample_rate;
        uint trigger_pin = pin_base + trig_channel;
        generate_pattern(pio1, 1, pattern, GENERATOR_PIN_BASE, 1250.0);
        dma_chan = 2; //dma_claim_unused_channel (true);
        printf("DMA channel %d\n",dma_chan);
        if(run_analyzer(8, num_samples, pio, sm, pin_base, sample_div, dma_chan, trigger_pin, trig_type))
        {
            sampleRun = true;
            commandComplete = false;
        }
    }

}

void analyser_task()
{}

bool run_analyzer(uint pin_count, uint sample_count, PIO pio, uint sm, uint pin_base, float freq_div, uint dma_chan, uint trigger_pin, uint trigger_type)
{
    uint32_t word_count = ((pin_count * sample_count) + 31) / 32;
   
    logic_analyser_init(pio, sm, pin_base, pin_count, trigger_pin, trigger_type, freq_div);

    logic_analyser_arm(pio, sm, dma_chan, capture_buf+2, word_count, dma_irq);

    return true;
}

void dma_irq()
{
  dma_hw->ints0 = 1u << dma_chan;
  commandComplete = true;
  sampleRun = false;
}

void process_capture_result()
{
    char header[12];
    uint8_t* buffer = (uint8_t*)capture_buf;
 
    sprintf(header, "#6%06d", num_samples);
    for(int i=0;i<8;i++)
        buffer[i] = header[i];
        
    command_complete((uint8_t*)capture_buf, 8+num_samples);
}