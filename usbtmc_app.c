/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Nathan Conrad
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <strings.h>
#include <stdlib.h>     /* atoi */
#include "tusb.h"
#include "bsp/board.h"
#include "main.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#if (CFG_TUD_USBTMC_ENABLE_488)
static usbtmc_response_capabilities_488_t const
#else
static usbtmc_response_capabilities_t const
#endif
tud_usbtmc_app_capabilities  =
{
    .USBTMC_status = USBTMC_STATUS_SUCCESS,
    .bcdUSBTMC = USBTMC_VERSION,
    .bmIntfcCapabilities =
    {
        .listenOnly = 0,
        .talkOnly = 0,
        .supportsIndicatorPulse = 1
    },
    .bmDevCapabilities = {
        .canEndBulkInOnTermChar = 0
    },

#if (CFG_TUD_USBTMC_ENABLE_488)
    .bcdUSB488 = USBTMC_488_VERSION,
    .bmIntfcCapabilities488 =
    {
        .supportsTrigger = 1,
        .supportsREN_GTL_LLO = 0,
        .is488_2 = 1
    },
    .bmDevCapabilities488 =
    {
      .SCPI = 1,
      .SR1 = 0,
      .RL1 = 0,
      .DT1 =0,
    }
#endif
};

#define IEEE4882_STB_QUESTIONABLE (0x08u)
#define IEEE4882_STB_MAV          (0x10u)
#define IEEE4882_STB_SER          (0x20u)
#define IEEE4882_STB_SRQ          (0x40u)

static const char idn[] = "TinyUSB,ModelNumber,SerialNumber,FirmwareVer123456\r\n";
static const char opc_1[] = "1\r\n";
static const char opc_0[] = "0\r\n";

static bool commandComplete;
static bool sampleRun;
uint8_t count = 0;

//static const char idn[] = "TinyUSB,ModelNumber,SerialNumber,FirmwareVer and a bunch of other text to make it longer than a packet, perhaps? lets make it three transfers...\n";
static volatile uint8_t status;

// 0=not query, 1=queried, 2=delay,set(MAV), 3=delay 4=ready?
// (to simulate delay)
enum  _states {
  QStart = 0,
  QDelayStart,
  QDelayRun,
  QDelayEnd,
  QSendResult
};

static volatile enum _states queryState = QStart;

static volatile uint32_t queryDelayStart;
static volatile uint32_t bulkInStarted;
static volatile uint32_t idnQuery;
static volatile uint32_t opcQuery;
static volatile uint32_t waveQuery;
volatile int num_samples = 0;
volatile float sample_rate = 1000.0;

static uint32_t resp_delay = 125u; // Adjustable delay, to allow for better testing
static size_t buffer_len;
static size_t buffer_tx_ix; // for transmitting using multiple transfers
static uint8_t buffer[225]; // A few packets long should be enough.

uint32_t *capture_buf = 0;


static usbtmc_msg_dev_dep_msg_in_header_t rspMsg = {
    .bmTransferAttributes =
    {
      .EOM = 1,
      .UsingTermChar = 0
    }
};

void tud_usbtmc_open_cb(uint8_t interface_id)
{
  (void)interface_id;
  tud_usbtmc_start_bus_read();
}

#if (CFG_TUD_USBTMC_ENABLE_488)
usbtmc_response_capabilities_488_t const *
#else
usbtmc_response_capabilities_t const *
#endif
tud_usbtmc_get_capabilities_cb()
{
  return &tud_usbtmc_app_capabilities;
}


bool tud_usbtmc_msg_trigger_cb(usbtmc_msg_generic_t* msg) {
  (void)msg;
  // Let trigger set the SRQ
  status |= IEEE4882_STB_SRQ;
  return true;
}

bool tud_usbtmc_msgBulkOut_start_cb(usbtmc_msg_request_dev_dep_out const * msgHeader)
{
  (void)msgHeader;
  buffer_len = 0;
  if(msgHeader->TransferSize > sizeof(buffer))
  {

    return false;
  }
  return true;
}

bool tud_usbtmc_msg_data_cb(void *data, size_t len, bool transfer_complete)
{
  // If transfer isn't finished, we just ignore it (for now)

  if(len + buffer_len < sizeof(buffer))
  {
    memcpy(&(buffer[buffer_len]), data, len);
    buffer_len += len;
  }
  else
  {
    return false; // buffer overflow!
  }
  if(transfer_complete)
    queryState = QDelayStart;
  else
    queryState = QStart;
  //queryState = transfer_complete;
  
  idnQuery = 0;
  opcQuery=0;
  waveQuery=0;

  if(transfer_complete && (len >=4) && !strncasecmp("*idn?",data,4))
  {
    idnQuery = 1;
    led_indicator_pulse();
  }
  if(transfer_complete && (len >=4) && !strncasecmp("*opc?",data,4))
  {
    opcQuery = 1;
  }
  if(transfer_complete && (len >=5) && !strncasecmp("rate ",data,5))
  {
    queryState = QStart;
    sample_rate = atof((char*)data + 5);
  }
  if(transfer_complete && (len >=9) && strncasecmp("l:capture ",data,9) == 0)
  {
    PIO pio = pio0;
    uint sm = 0;
    uint dma_chan = 0;
    num_samples = tu_max32(atoi((char*)data + 10), 1);
    float sample_div = (float) clock_get_hz(clk_sys) / sample_rate;
    
    if(!run_analyzer(8, num_samples, pio, sm, 15, sample_div, dma_chan,true))
    {
      queryState = QStart;
    }
    else
    {
      sampleRun = false;
      commandComplete = false;
    }
  }
  if(transfer_complete && (len >=5) && !strncasecmp("data?",data,5))
  {
    waveQuery = 1;
  }
  if(transfer_complete && !strncasecmp("delay ",data,5))
  {
    queryState = QStart;
    int d = atoi((char*)data + 5);
    if(d > 10000)
      d = 10000;
    if(d<0)
      d=0;
    resp_delay = (uint32_t)d;
  }
  tud_usbtmc_start_bus_read();
  return true;
}

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

bool tud_usbtmc_msgBulkIn_complete_cb()
{
  if((buffer_tx_ix == buffer_len) || idnQuery) // done
  {
    status &= (uint8_t)~(IEEE4882_STB_MAV); // clear MAV
    queryState = QStart;
    bulkInStarted = 0;
    buffer_tx_ix = 0;
  }
  tud_usbtmc_start_bus_read();

  return true;
}

static unsigned int msgReqLen;

bool tud_usbtmc_msgBulkIn_request_cb(usbtmc_msg_request_dev_dep_in const * request)
{
  rspMsg.header.MsgID = request->header.MsgID,
  rspMsg.header.bTag = request->header.bTag,
  rspMsg.header.bTagInverse = request->header.bTagInverse;
  msgReqLen = request->TransferSize;

#ifdef xDEBUG
  uart_tx_str_sync("MSG_IN_DATA: Requested!\r\n");
#endif
  if(queryState == QStart || (buffer_tx_ix == 0))
  {
    TU_ASSERT(bulkInStarted == 0);
    bulkInStarted = 1;

    // > If a USBTMC interface receives a Bulk-IN request prior to receiving a USBTMC command message
    //   that expects a response, the device must NAK the request (*not stall*)
  }
  else
  {
    size_t txlen = tu_min32(buffer_len-buffer_tx_ix,msgReqLen);
    tud_usbtmc_transmit_dev_msg_data(&buffer[buffer_tx_ix], txlen,
        (buffer_tx_ix+txlen) == buffer_len, false);
    buffer_tx_ix += txlen;
  }
  // Always return true indicating not to stall the EP.
  return true;
}

void dma_irq()
{
  dma_hw->ints0 = 1u << 0;
  commandComplete = true;
  sampleRun = false;

}

void usbtmc_app_task_iter(void) {
  switch(queryState) {
  case QStart:
    break;
  case QDelayStart:
    queryDelayStart = board_millis();
    queryState = QDelayRun;
    break;
  case QDelayRun:
    if( (board_millis() - queryDelayStart) > resp_delay) {
      queryDelayStart = board_millis();
      queryState=QDelayEnd;
      status |= 0x10u; // MAV
      status |= 0x40u; // SRQ
    }
    break;
  case QDelayEnd:
    if( (board_millis() - queryDelayStart) > resp_delay) {
      queryState = QSendResult;
    }
    break;
  case QSendResult: // time to transmit;
    if(bulkInStarted && (buffer_tx_ix == 0)) {
      if(idnQuery)
      {
        tud_usbtmc_transmit_dev_msg_data(idn,  tu_min32(sizeof(idn)-1,msgReqLen),true,false);
        queryState = QStart;
        bulkInStarted = 0;
      }
      else if(opcQuery)
      {
        if(commandComplete)
          tud_usbtmc_transmit_dev_msg_data(opc_1, tu_min32(sizeof(opc_1)-1,msgReqLen),true,false);
        else
          tud_usbtmc_transmit_dev_msg_data(opc_0, tu_min32(sizeof(opc_0)-1,msgReqLen),true,false);
        
        queryState = QStart;
        bulkInStarted = 0;
      }
      else if(waveQuery)
      {
        uint8_t header[12];
        uint8_t* buffer = (uint8_t*)capture_buf;
        sprintf(header, "#6%06d", num_samples);
        for(int i=0;i<8;i++)
          buffer[i] = header[i];
        
        tud_usbtmc_transmit_dev_msg_data((uint8_t*)capture_buf, tu_min32(8+num_samples, msgReqLen), true, false);
        queryState = QStart;
        bulkInStarted = 0;
      }
      else
      {
        buffer_tx_ix = tu_min32(buffer_len,msgReqLen);
        tud_usbtmc_transmit_dev_msg_data(buffer, buffer_tx_ix, buffer_tx_ix == buffer_len, false);
      }
      // MAV is cleared in the transfer complete callback.
    }
    break;
  default:
    TU_ASSERT(false,);
    return;
  }
}

bool tud_usbtmc_initiate_clear_cb(uint8_t *tmcResult)
{
  *tmcResult = USBTMC_STATUS_SUCCESS;
  queryState = QStart;
  bulkInStarted = false;
  status = 0;
  return true;
}

bool tud_usbtmc_check_clear_cb(usbtmc_get_clear_status_rsp_t *rsp)
{
  queryState = QStart;
  bulkInStarted = false;
  status = 0;
  buffer_tx_ix = 0u;
  buffer_len = 0u;
  rsp->USBTMC_status = USBTMC_STATUS_SUCCESS;
  rsp->bmClear.BulkInFifoBytes = 0u;
  return true;
}
bool tud_usbtmc_initiate_abort_bulk_in_cb(uint8_t *tmcResult)
{
  bulkInStarted = 0;
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}
bool tud_usbtmc_check_abort_bulk_in_cb(usbtmc_check_abort_bulk_rsp_t *rsp)
{
  (void)rsp;
  tud_usbtmc_start_bus_read();
  return true;
}

bool tud_usbtmc_initiate_abort_bulk_out_cb(uint8_t *tmcResult)
{
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;

}
bool tud_usbtmc_check_abort_bulk_out_cb(usbtmc_check_abort_bulk_rsp_t *rsp)
{
  (void)rsp;
  tud_usbtmc_start_bus_read();
  return true;
}

void tud_usbtmc_bulkIn_clearFeature_cb(void)
{
}
void tud_usbtmc_bulkOut_clearFeature_cb(void)
{
  tud_usbtmc_start_bus_read();
}

// Return status byte, but put the transfer result status code in the rspResult argument.
uint8_t tud_usbtmc_get_stb_cb(uint8_t *tmcResult)
{
  uint8_t old_status = status;
  status = (uint8_t)(status & ~(IEEE4882_STB_SRQ)); // clear SRQ

  *tmcResult = USBTMC_STATUS_SUCCESS;
  // Increment status so that we see different results on each read...

  return old_status;
}

bool tud_usbtmc_indicator_pulse_cb(tusb_control_request_t const * msg, uint8_t *tmcResult)
{
  (void)msg;
  led_indicator_pulse();
  *tmcResult = USBTMC_STATUS_SUCCESS;
  return true;
}
