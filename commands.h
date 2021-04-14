#ifndef __COMMANDS__H__
#define __COMMANDS__H__

#ifdef NDEBUG
    #define MAX_BUFFER_SIZE 50002
#else
    #define MAX_BUFFER_SIZE 49002
#endif

static const char idn[] = "Rasp Pico Logic,1.0,1000,v1.0\r\n";
static const char opc_1[] = "1\r\n";
static const char opc_0[] = "0\r\n";
static bool commandComplete;
uint32_t capture_buf[MAX_BUFFER_SIZE];

uint8_t* esr_buf=0;
volatile int num_samples = 0;
volatile float sample_rate = 1000.0;
volatile uint pattern=0;
static bool sampleRun;
static uint32_t status_register;
uint trig_channel=0;
uint trig_type=0;

bool command_complete(uint8_t const *aBuffer, size_t aLen);
void process_capture_result();
void process_opc();
void process_esr();
void analyser_task();
bool run_analyzer(uint pin_count, uint sample_count, PIO pio, uint sm, uint pin_base, float freq_div, uint dma_chan, uint trigger_pin, uint trigger_type);
#endif
