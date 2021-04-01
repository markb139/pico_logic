#include <stdio.h>
#include <string.h>
#include <stdlib.h>   
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "main.h"
#include "commands.h"

static inline uint32_t tu_max32 (uint32_t x, uint32_t y) { return (x > y) ? x : y; }

bool process_command(uint8_t* aData, size_t aLen)
{
    if(aLen >=4 && !strncasecmp("*idn?", aData,4))
    {
        led_indicator_pulse();
        command_complete(idn, strlen(idn));
    }
    if(aLen >=4 && !strncasecmp("*opc?", aData,4))
    {
        process_opc();
    }
    if(aLen >=4 && !strncasecmp("*esr?", aData,4))
    {
        process_esr();
    }
    if(aLen >=9 && strncasecmp("l:capture ", aData,9) == 0)
    {
        PIO pio = pio0;
        uint sm = 0;
        uint dma_chan = 0;
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
        
            if(run_analyzer(8, num_samples, pio, sm, 15, sample_div, dma_chan,true))
            {
                sampleRun = true;
                commandComplete = false;
            }
        }
    }
    if(aLen >=5 && !strncasecmp("rate ",aData,5))
    {
        sample_rate = atof((char*) aData + 5);
    }

    if(aLen >=5 && !strncasecmp("data?", aData,5))
    {
        process_capture_result();
    }
    
    return true;
}

void process_opc()
{
    if(commandComplete)
        command_complete(opc_1, strlen(opc_1));
    else
        command_complete(opc_0, strlen(opc_0));
}

void process_esr()
{
    if(capture_buf)
    {
        free(capture_buf);
    }
     capture_buf = malloc(32);

    sprintf((uint8_t*)capture_buf, "%d\r\n", status_register);
    command_complete((uint8_t*)capture_buf, strlen((uint8_t*)capture_buf));
    status_register = 0;
}

void analyser_task()
{}

bool run_analyzer(uint pin_count, uint sample_count, PIO pio, uint sm, uint pin_base, float freq_div, uint dma_chan, bool trigger)
{
  uint32_t word_count = ((pin_count * sample_count) + 31) / 32;
  uint32_t capture_buf_memory_size = word_count * sizeof(uint32_t);
  if(capture_buf)
  {
    free(capture_buf);
  }
  capture_buf = malloc(8 + capture_buf_memory_size);
  if (capture_buf == NULL) {
      return false;
  }

  logic_analyser_init(pio, sm, pin_base, pin_count, freq_div);

  logic_analyser_arm(pio, sm, dma_chan, capture_buf, word_count, pin_base, trigger, dma_irq);

  return true;
}

void dma_irq()
{
  dma_hw->ints0 = 1u << 0;
  commandComplete = true;
  sampleRun = false;

}

void process_capture_result()
{
    uint8_t header[12];
    uint8_t* buffer = (uint8_t*)capture_buf;
    sprintf(header, "#6%06d", num_samples);
    for(int i=0;i<8;i++)
        buffer[i] = header[i];
        
    command_complete((uint8_t*)capture_buf, 8+num_samples);
}