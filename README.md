# pico_logic
Simple USBTMC & VXI capable, Raspberry Pi Pico logic analyser. Can work with Sigrok

## Introduction
This simple project enables the Raspberry Pi Pico board to act as a logic capture device that can work with SIGROK.
There are two builds included in the project. usbtmc for usb connections. And vxitmc for Wifi connections

## Pico W support
The vxitmc build is designed to work with the Pico W and an external EEPROM connected via I2C. The EEPROM is used for storing Wifi credntials.
