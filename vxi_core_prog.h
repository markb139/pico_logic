#ifndef __VXI_CORE_PROH_H__
#define __VXI_CORE_PROH_H__

uint max_read_size;
uint chunk_offset;

typedef struct DEVICE_WRITE_PARAMS_T_ {
    uint32_t link_id;
    uint32_t io_timeout;
    uint32_t lock_timeout;
    uint32_t flags;
    void* data;
} DEVICE_WRITE_PARAMS_T;

typedef struct DEVICE_WRITE_PARAMS_REPLY_T_ {
    uint32_t error;
    uint32_t size;
} DEVICE_WRITE_PARAMS_REPLY_T;

typedef struct DEVICE_READ_PARAMS_T_ {
    uint32_t link_id;
    uint32_t size;
    uint32_t io_timeout;
    uint32_t lock_timeout;
} DEVICE_READ_PARAMS_T;

typedef struct DEVICE_READ_PARAMS_REPLY_T_ {
    uint32_t error;
    uint32_t reason;
} DEVICE_READ_PARAMS_REPLY_T;

typedef struct CREATE_LINK_PARAMS_T_ {
    uint32_t client_id;
    uint32_t lock_device;
    uint32_t lock_timeout;
    void* device_name;
} CREATE_LINK_PARAMS_T;

typedef struct CREATE_LINK_PARAMS_REPLY_T_ {
    uint32_t error;
    uint32_t link_id;
    uint32_t abort_port;
    uint32_t message_max_length;
} CREATE_LINK_PARAMS_REPLY_T;

///
typedef struct DESTROY_LINK_PARAMS_T_ {
    uint32_t link_id;
} DESTROY_LINK_PARAMS_T;

typedef struct DESTROY_LINK_PARAMS_REPLY_T_ {
    uint32_t error;
} DESTROY_LINK_PARAMS_REPLY_T;

err_t decode_vxi(TCP_SERVER_T *state, struct tcp_pcb *tpcb, TCP_RPC_T* rpc_call, uint32_t* buffer);

#endif