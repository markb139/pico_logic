#ifndef MAIN_H
#define MAIN_H
#include "hardware/pio.h"
#include "hardware/irq.h"

void led_indicator_pulse(void);
void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, uint trigger_pin, uint trigger_type, float div);
void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words, irq_handler_t dma_handler);
void generate_pattern(PIO pio, uint sm, uint pattern, float div);
void dma_irq();
#endif
