#ifndef __EEPROM_24LC02b_H__
#define __EEPROM_24LC02b_H__

int initialise_eeprom(uint data_pin, uint clock_pin);
int eeprom_read_block(uint8_t address, uint8_t *data);
void dump_block(uint8_t* data);

#endif