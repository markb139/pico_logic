#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__

#define TCP_PORT 111
#define BUF_SIZE 128
#define POLL_TIME_S 5
#define DEBUG_printf 

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    bool complete;
    uint32_t buffer_recv[BUF_SIZE];
    int recv_len;
} TCP_SERVER_T;

const uint8_t *responseBuffer;
uint responseBufferLen;


typedef struct CREDENTIALS_T_ {
        uint32_t flavor;
        uint32_t length;
} CREDENTIALS_T;

typedef struct VERIFIER_T_ {
        uint32_t flavor;
        uint32_t length;
} VERIFIER_T;

typedef struct TCP_RPC_T_ {
    uint32_t header;
    uint32_t xid;
    uint32_t msg_type;
    uint32_t rpc_version;
    uint32_t program;
    uint32_t version;
    uint32_t procedure;
    CREDENTIALS_T credentials;
    VERIFIER_T verifier;
    void* the_rest;
} TCP_RPC_T;

typedef struct TCP_RPC_REPLY_T_ {
    uint32_t header;
    uint32_t xid;
    uint32_t msg_type;
    uint32_t state;
    uint32_t version;
    uint32_t version_len;
    uint32_t acceptance_state;
} TCP_RPC_REPLY_T;

typedef struct GETPORT_REPLY_T_ {
    uint32_t port;
} GETPORT_REPLY_T;

typedef struct PADDED_STRING_T_ {
    uint32_t length;
    uint8_t* contents;
    uint8_t* fill_bytes;
} PADDED_STRING_T;

typedef struct send_t_ {
    void* ptr;
    uint32_t length;
    uint32_t flags;
} SEND_T;

err_t rpc_server_start(void);
uint decode_string(void* buffer, uint8_t* string_data);
uint encode_string(const uint8_t* str, const uint str_len, void* buffer);
void create_rpc_reply(TCP_RPC_REPLY_T* rpc_reply, uint32_t xid, uint32_t length);
err_t send_data_list(struct tcp_pcb *tpcb, SEND_T * data, uint length);

#endif