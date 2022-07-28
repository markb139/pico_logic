#include <stdlib.h>
#include <pico/stdlib.h>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

#include "rpc_server.h"
#include "vxi_core_prog.h"

#define MAX_READ_SIZE 2048

uint32_t get_linkparams(uint32_t* buffer);
uint32_t create_link(uint32_t client_id);
void get_destroy_link_params(uint32_t* buffer);
int get_device_write_params(uint32_t* buffer);
uint get_device_read(uint offset, uint maxlen);
void get_device_read_params(uint32_t* buffer);

bool process_command(uint8_t* aData, size_t aLen);
uint encode_string_no_copy(const uint8_t* str, const uint str_len, PADDED_STRING_T* string);

bool command_complete(uint8_t const *aBuffer, size_t aLen)
{
    responseBuffer = aBuffer;
    responseBufferLen = aLen;
}

#define CREATE_LINK 10
#define DESTROY_LINK 23
#define DEVICE_WRITE 11
#define DEVICE_READ 12

err_t decode_vxi(TCP_SERVER_T *state, struct tcp_pcb *tpcb, TCP_RPC_T* rpc_call, uint32_t* buffer)
{
    TCP_RPC_REPLY_T rpc_reply;
    uint procedure = htonl(rpc_call->procedure);

    if(procedure == CREATE_LINK)
    {
        DEBUG_printf("CREATE LINK\n");
        SEND_T send_data[2];

        uint32_t client_id = get_linkparams(buffer+11);
        uint32_t link_id = create_link(client_id);
        CREATE_LINK_PARAMS_REPLY_T create_reply;
        
        create_rpc_reply(&rpc_reply, rpc_call->xid, sizeof(TCP_RPC_REPLY_T) + sizeof(CREATE_LINK_PARAMS_REPLY_T) - 4);
        
        send_data[0].ptr = (void*)&rpc_reply;
        send_data[0].length = sizeof(TCP_RPC_REPLY_T);
        send_data[0].flags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;

        create_reply.error = 0;
        create_reply.link_id  = htonl(link_id);
        create_reply.abort_port = htonl(333);
        create_reply.message_max_length = htonl(1024);

        send_data[1].ptr = (void*)&create_reply;
        send_data[1].length = sizeof(CREATE_LINK_PARAMS_REPLY_T);
        send_data[1].flags = TCP_WRITE_FLAG_COPY;
        
        return send_data_list(tpcb, send_data, 2);
    }
    else if (procedure == DESTROY_LINK)
    {
        DEBUG_printf("DESTROY LINK\n");
        SEND_T send_data[2];
        get_destroy_link_params(buffer+11);
        // uint len = destroy_link(htonl(prog),);
        DESTROY_LINK_PARAMS_REPLY_T destroy_reply;

        create_rpc_reply(&rpc_reply, rpc_call->xid, sizeof(TCP_RPC_REPLY_T) + sizeof(DESTROY_LINK_PARAMS_REPLY_T) - 4);

        send_data[0].ptr = (void*)&rpc_reply;
        send_data[0].length = sizeof(TCP_RPC_REPLY_T);
        send_data[0].flags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;

        destroy_reply.error = 0;
        send_data[1].ptr = (void*)&destroy_reply;
        send_data[1].length = sizeof(DESTROY_LINK_PARAMS_REPLY_T);
        send_data[1].flags = TCP_WRITE_FLAG_COPY;

        return send_data_list(tpcb, send_data, 2);
    }
    else if (procedure == DEVICE_WRITE)
    {
        DEBUG_printf("DEVICE WRITE\n");
        SEND_T send_data[2];

        chunk_offset = 0;
        int written_size = get_device_write_params(buffer+11);
        DEVICE_WRITE_PARAMS_REPLY_T device_write_reply;
        create_rpc_reply(&rpc_reply, rpc_call->xid, sizeof(TCP_RPC_REPLY_T) + sizeof(DEVICE_WRITE_PARAMS_REPLY_T) - 4);

        send_data[0].ptr = (void*)&rpc_reply;
        send_data[0].length = sizeof(TCP_RPC_REPLY_T);
        send_data[0].flags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;

        device_write_reply.error = 0;
        device_write_reply.size = htonl(written_size);

        send_data[1].ptr = (void*)&device_write_reply;
        send_data[1].length = sizeof(DEVICE_WRITE_PARAMS_REPLY_T);
        send_data[1].flags = TCP_WRITE_FLAG_COPY;

        return send_data_list(tpcb, send_data, 2);

    }
    else if (procedure == DEVICE_READ)
    {
        DEBUG_printf("DEVICE READ\n");
        SEND_T send_data[5];

        static uint8_t fill[4] = {0,0,0,0};
        get_device_read_params(buffer+11);
        DEVICE_READ_PARAMS_REPLY_T device_read_reply;

        uint reply_len = get_device_read(chunk_offset, max_read_size);
        uint32_t string_length = htonl(reply_len);

        DEBUG_printf("Sending %d bytes from %d\n", reply_len, chunk_offset);
        DEBUG_printf("responseBufferLen %d\n", responseBufferLen);

        uint fill_bytes_size = (4 - (reply_len % 4)) % 4;
        DEBUG_printf("fill bytes %d bytes\n", fill_bytes_size);

        create_rpc_reply(&rpc_reply, rpc_call->xid, sizeof(TCP_RPC_REPLY_T) + sizeof(DEVICE_READ_PARAMS_REPLY_T) + reply_len + fill_bytes_size);

        send_data[0].ptr = (void*)&rpc_reply;
        send_data[0].length = sizeof(TCP_RPC_REPLY_T);
        send_data[0].flags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;

        device_read_reply.error = 0;
        device_read_reply.reason = htonl(4);

        send_data[1].ptr = (void*)&device_read_reply;
        send_data[1].length = sizeof(DEVICE_READ_PARAMS_REPLY_T);
        send_data[1].flags = TCP_WRITE_FLAG_COPY  | TCP_WRITE_FLAG_MORE;

        send_data[2].ptr = (void*)&(string_length);
        send_data[2].length = sizeof(uint32_t);
        send_data[2].flags = TCP_WRITE_FLAG_COPY  | TCP_WRITE_FLAG_MORE;

        uint flag = TCP_WRITE_FLAG_MORE;

        if(fill_bytes_size == 0)
            flag = 0;

        send_data[3].ptr = (void*)(responseBuffer+chunk_offset);
        send_data[3].length = reply_len;
        send_data[3].flags = flag;

        chunk_offset+=reply_len;

        if(fill_bytes_size != 0)
        {
            DEBUG_printf("sending fill bytes %d bytes\n", fill_bytes_size);
            send_data[4].ptr = (void*)&fill;
            send_data[4].length = fill_bytes_size;
            send_data[4].flags = TCP_WRITE_FLAG_COPY;

            return send_data_list(tpcb, send_data, 5);
        }
        else
            return send_data_list(tpcb, send_data, 4);
    }
    return ERR_OK;
}

uint32_t get_linkparams(uint32_t* buffer)
{
    uint8_t device_name[64];

    CREATE_LINK_PARAMS_T* link_params = (CREATE_LINK_PARAMS_T*)buffer;
    link_params->client_id = link_params->client_id;
    decode_string(&link_params->device_name, device_name);
    return link_params->client_id;
}

uint32_t create_link(uint32_t client_id)
{
    return 5678;
}

void get_destroy_link_params(uint32_t* buffer)
{
    DESTROY_LINK_PARAMS_T* link_params = (DESTROY_LINK_PARAMS_T*)buffer;
    link_params->link_id = htonl(link_params->link_id);
}

int get_device_write_params(uint32_t* buffer)
{
    uint8_t* write_data = (uint8_t*) malloc(128);
    DEVICE_WRITE_PARAMS_T* device_write_params = (DEVICE_WRITE_PARAMS_T*)buffer;
    size_t len = decode_string(&device_write_params->data, write_data);
    bool pc = process_command(write_data, len);
    free(write_data);
    DEBUG_printf("process_command %d\n", pc);
    return len;
}

void get_device_read_params(uint32_t* buffer)
{
    DEVICE_READ_PARAMS_T* device_read_params = (DEVICE_READ_PARAMS_T*)buffer;
    max_read_size = MAX_READ_SIZE;
    DEBUG_printf("Read device %d size %d io timeout %d lock timeout %d\n", htonl(device_read_params->link_id), 
                                                                            max_read_size, 
                                                                            htonl(device_read_params->io_timeout),
                                                                            htonl(device_read_params->lock_timeout));
}

uint get_device_read(uint offset, uint maxlen)
{
    uint buffer_left = responseBufferLen - offset;
    return MIN(maxlen, buffer_left);
}
