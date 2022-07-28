#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/apps/mdns.h"

#include "rpc_server.h"
#ifdef ENABLE_EEPROM
#include "eeprom_24lc02b.h"
#endif

#define DEBUG_printf printf
#define TEST_ITERATIONS 10
#define POLL_TIME_S 5

#define I2C_SDA_PIN 18
#define I2C_SCL_PIN 19

typedef struct WIFI_DATA_T_ {
    uint8_t ssid[128];
    uint8_t pwd[128];
} WIFI_DATA_T;

static WIFI_DATA_T wifi_data;

void get_wifi_credentials()
{
#ifdef WIFI_SSID 
        strcpy(wifi_data.ssid, WIFI_SSID);
        strcpy(wifi_data.pwd, WIFI_PWD);
#endif
#ifdef ENABLE_EEPROM
        initialise_eeprom(I2C_SDA_PIN, I2C_SCL_PIN);
        uint8_t raw_data[256];
        if(eeprom_read_block(0, raw_data) >0)
        {
            dump_block(raw_data);
            strcpy(wifi_data.ssid, &raw_data[0]);
            strcpy(wifi_data.pwd, &raw_data[128]);
            DEBUG_printf("Wifi: %s %s\n",wifi_data.ssid, wifi_data.pwd);
        }
        else
        {
            gpio_put(21, 1);
            printf("FAILED TO READ EEPROM\n");
        }
#endif
    
}

void initialise()
{
    stdio_init_all();
    printf("STARTING\n");
    gpio_init(20);
    gpio_set_dir(20, GPIO_OUT);
    gpio_init(21);
    gpio_set_dir(21, GPIO_OUT);
    gpio_put(20, 0);
    gpio_put(21, 0);
    if (cyw43_arch_init()) 
    {
        printf("failed to initialise\n");
        gpio_put(21, 1);
    }
    else
    {
        cyw43_arch_enable_sta_mode();
    }
}

void deinitialise()
{}

err_t start_networking()
{
    err_t err = ERR_OK;
    get_wifi_credentials();
    printf("Connecting to WiFi...\n");
    err = cyw43_arch_wifi_connect_timeout_ms(wifi_data.ssid, wifi_data.pwd, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (err)
    {
        gpio_put(21, 1);
        printf("failed to connect. %d\n", err);
    } 
    else 
    {
        printf("Connected.\n");
        err = rpc_server_start();
        if(err != ERR_OK)
        {
            DEBUG_printf("Failed to run\n");
        }
    }
    return err;
}

int main()
{
    int err = 0;
    initialise();
    err = start_networking();
    if(err != ERR_OK)
    {
        printf("!! start_networking failed %d !!\n", err);
    }
 
    cyw43_arch_deinit();
    deinitialise();
    printf("\n\nDONE\n\n");
    return 0;
}