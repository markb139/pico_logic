#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

/****************************************************
 * External EEPROM handler
 ***************************************************/
#define EEPROM_I2C_ADDRESS 0X50

int eeprom_write(uint8_t address, uint8_t data)
{
    uint8_t buffer[2];

    buffer[0] = address;
    buffer[1] = data;
    return i2c_write_blocking(i2c1, EEPROM_I2C_ADDRESS, buffer, 2, false);
}

int eeprom_write_block_(uint8_t address, uint8_t *data, int length)
{
    uint8_t *buffer = malloc(1+length);

    buffer[0] = address;
    memcpy(buffer+1, data, length);
    
    int ret = i2c_write_blocking(i2c1, EEPROM_I2C_ADDRESS, buffer, 1+length, false);
    free(buffer);
    return ret;
}

int eeprom_write_block(uint8_t address, uint8_t *data, int length)
{
    // only 8 bytes at a time
    int ret = 0;
    uint8_t eights = length / 8;
    uint8_t left_over = length % 8;
    for(int i=0;i<eights; i++)
    {
        uint8_t add = address + i*8;
        uint8_t *data_ptr = data + (i*8);
        int bytes_written = eeprom_write_block_(add, data_ptr, 8);
        if(bytes_written >0)
        {
            ret += bytes_written;
        }
        else
        {
            printf("FAILED %d at %d\n", bytes_written, add);
        }
        // wait a bit. Is there a way to acknowledge a write ?
        sleep_ms(5);
    }
    if(left_over > 0)
    {
        ret += eeprom_write_block_(address + left_over + eights*8, data+ (left_over + eights*8), left_over);
    }
    return ret;
}

int eeprom_read(uint8_t address, uint8_t *data)
{
    uint8_t buffer[1];
    buffer[0] = address;
    int ret;
    ret = i2c_write_blocking(i2c1, EEPROM_I2C_ADDRESS, buffer, 1, true);
    if(ret == 1)
        ret = i2c_read_blocking(i2c1, EEPROM_I2C_ADDRESS, data, 1, false);

    return ret;
}

int eeprom_read_block(uint8_t address, uint8_t *data)
{
    uint8_t *buffer = malloc(257);
    buffer[0] = address;
    int ret;
    ret = eeprom_read(255, buffer);
    ret = i2c_read_blocking(i2c1, EEPROM_I2C_ADDRESS, buffer+1, 256, false);
    memcpy(data, buffer + 1, 256);
    free(buffer);
    return ret;
}

void dump_block(uint8_t* data)
{
    uint8_t ascii[9];
    ascii[8] = 0;

    for(int i=0; i<32; i++)
    {
        printf("0x%02x", i*8);
        for(int j=0; j<8; j++)
        {
            uint loc = j+ i*8;
            printf(" %02x", data[loc]);
            ascii[j] = data[loc] >= 32 && data[loc] <= 127 ? data[loc] : '.';
        }
        printf(" %s\n", ascii);
    }
}

int initialise_eeprom(uint data_pin, uint clock_pin)
{
    i2c_init(i2c1, 100 * 1000);
    gpio_set_function(data_pin, GPIO_FUNC_I2C);
    gpio_set_function(clock_pin, GPIO_FUNC_I2C);
    gpio_pull_up(data_pin);
    gpio_pull_up(clock_pin);
    return 0;
}
