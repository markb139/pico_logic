#include <stdlib.h>
#include <pico/stdlib.h>

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/apps/mdns.h"

#include "rpc_server.h"
#include "vxi_core_prog.h"

TCP_SERVER_T* tcp_server_init(void);
bool tcp_server_open(void *arg);
err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err);
err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
void tcp_server_err(void *arg, err_t err);
err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);
err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

uint get_address(uint32_t program, void* buffer);

err_t decode_buffer(struct tcp_pcb *tpcb, TCP_SERVER_T *state);

err_t rpc_server_start(void) 
{
    TCP_SERVER_T *state = tcp_server_init();
    if (!state) {
        return ERR_CONN;
    }
    if (!tcp_server_open(state)) {
        return ERR_CONN;
    }

    while(!state->complete) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
    
    free(state);
    return ERR_OK;
}

TCP_SERVER_T* tcp_server_init(void)
{
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    return state;
}

bool tcp_server_open(void *arg)
{
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    printf("Starting server at %s on port %u\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), TCP_PORT);
    gpio_put(20, 1);


    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        DEBUG_printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, NULL, TCP_PORT);
    if (err) {
        DEBUG_printf("failed to bind to port %d\n");
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb) {
        DEBUG_printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}

err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        DEBUG_printf("Failure in accept %d\n", err);
        return err;
    }
    DEBUG_printf("Client connected\n");

    state->client_pcb = client_pcb;
    tcp_arg(client_pcb, state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    DEBUG_printf("tcp_server_sent %u\n", len);
    
    return ERR_OK;
}

err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    uint16_t recevied_count;
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if ( p == NULL)
    {
        DEBUG_printf("EOF\n");
        tcp_close(state->client_pcb);
    }
    else if(p->tot_len > 0) 
    {
        DEBUG_printf("Received %d bytes\n", p->len);
        DEBUG_printf("tcp_server_recv %d/%d err %d\n", p->tot_len, state->recv_len, err);

        // Receive the buffer
        const uint16_t buffer_left = BUF_SIZE - state->recv_len;
        recevied_count = pbuf_copy_partial(p, state->buffer_recv + state->recv_len, p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
        if (recevied_count > 0)
        {
            state->recv_len += recevied_count;
        }
        else
        {
            DEBUG_printf("Error copying pbuf 0x%08x\n", p);
        }
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    // Have we have received the whole buffer
    if (state->recv_len == p->tot_len) {
        DEBUG_printf("Received all the bytes\n");
        state->recv_len = 0;
        err_t ret  = decode_buffer(tpcb, state);
        return ret;
    }
    return ERR_OK;
}

err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb)
{
    if(tpcb->state == CLOSE_WAIT )
    {
        TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
        if(state->client_pcb)
        {
            tcp_abort(state->client_pcb);
            tcp_sent(state->client_pcb, NULL);
            tcp_recv(state->client_pcb, NULL);
            tcp_poll(state->client_pcb, NULL, POLL_TIME_S * 2);
            tcp_err(state->client_pcb, NULL);
            state->client_pcb = NULL;
            DEBUG_printf("CLOSE_WAIT\n");
        }
    }
    return ERR_OK;
}

void tcp_server_err(void *arg, err_t err)
{
    DEBUG_printf("tc p_client_err_fn %d\n", err);
}

err_t decode_buffer(struct tcp_pcb *tpcb, TCP_SERVER_T *state)
{
    uint32_t* buffer = state->buffer_recv;
    TCP_RPC_T* rpc_call = (TCP_RPC_T*)buffer;
    TCP_RPC_REPLY_T rpc_reply;
    uint32_t* ptr32 = (uint32_t*)(&rpc_call->the_rest);
    uint32_t prog = *ptr32;
    uint program = htonl(rpc_call->program);
    DEBUG_printf("RPC CALL: xid=0x%08x\n program=0x%08x\n procedure=%d\n portmap prog=%d\n", htonl(rpc_call->xid), htonl(rpc_call->program), htonl(rpc_call->procedure), htonl(prog));
    if (program == 100000)
    {
        if(htonl(rpc_call->procedure) == 3)
        {
            DEBUG_printf("GETADDR\n");
            SEND_T send_data[2];
            uint32_t address[16];
            uint len = get_address(htonl(prog), (void*) &address);
            create_rpc_reply(&rpc_reply, rpc_call->xid, sizeof(TCP_RPC_REPLY_T) - 4 +len);

            send_data[0].ptr = (void*)&rpc_reply;
            send_data[0].length = sizeof(TCP_RPC_REPLY_T);
            send_data[0].flags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;

            send_data[1].ptr = (void*)&address;
            send_data[1].length = len;
            send_data[1].flags = TCP_WRITE_FLAG_COPY;
                
            return send_data_list(tpcb, send_data, 2);
        }
    }
    else if (program == 395183)
    {
        return decode_vxi(state, tpcb, rpc_call, buffer);
    }
    else
    {
        DEBUG_printf("Uknown call prog %d -> procedure %d\n", rpc_call->program, rpc_call->procedure);
    }
    
    DEBUG_printf("<-decode_buffer ERR_OK\n");
    return ERR_OK;
}

uint get_address(uint32_t program, void* buffer)
{
    const uint8_t* address = "192.168.1.46.0.111";
    DEBUG_printf("Get address %d\n", program);
    return encode_string(address, strlen(address), buffer);
}

uint encode_string(const uint8_t* str, const uint str_len, void* buffer)
{
    uint8_t str_padding = str_len % 4;
    DEBUG_printf("str len %d padding %d\n", str_len, str_padding);
    *((uint32_t*)buffer) = htonl(str_len);
    memcpy((uint8_t*)(buffer+4), str, str_len);
    if(str_padding > 0)
    {
        str_padding = 4 - str_padding;
        DEBUG_printf("padding %d\n", str_padding);
        memset((uint8_t*)(buffer+4+str_len), 0xff, str_padding);
    }

    return 4+str_padding+str_len;
}

uint decode_string(void* buffer, uint8_t* string_data)
{
    PADDED_STRING_T* string = (PADDED_STRING_T*)buffer;

    memcpy(string_data, &string->contents, htonl(string->length));
    string_data[htonl(string->length)] = 0;
    DEBUG_printf("Decoded string len=%d \"%s\"\n", htonl(string->length), string_data);
    return htonl(string->length);
}

void create_rpc_reply(TCP_RPC_REPLY_T* rpc_reply, uint32_t xid, uint32_t length)
{
    rpc_reply->header = htonl(0x80000000 + length);
    rpc_reply->xid = xid;
    rpc_reply->msg_type = htonl(1);
    rpc_reply->state = 0;
    rpc_reply->version = 0;
    rpc_reply->version_len = 0;
    rpc_reply->acceptance_state = 0;
}

err_t send_data_list(struct tcp_pcb *tpcb, SEND_T * data, uint length)
{
    for(uint i=0; i<length; i++)
    {
        err_t err = tcp_write(tpcb, data[i].ptr, data[i].length, data[i].flags);
        if (err != ERR_OK) 
        {
            DEBUG_printf("Failed to write data %d\n", err);
            return err;
        }
    }
}
