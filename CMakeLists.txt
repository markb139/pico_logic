# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD "pico_w")

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/pi/pico/pico-sdk")

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(usbtmc C CXX)

# Initialise the Pico SDK
pico_sdk_init()

# add the analyser library
add_subdirectory(logicanalyser-lib)

# Add executable. Default name is the project name, version 0.1
add_executable(usbtmc 
        main.c 
        usb_descriptors.c 
        usbtmc_app.c 
        commands.c
)

target_include_directories(usbtmc PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_set_program_name(usbtmc "usbtmc")
pico_set_program_version(usbtmc "0.1")

pico_enable_stdio_uart(usbtmc 1)
pico_enable_stdio_usb(usbtmc 0)

target_link_libraries(usbtmc 
                pico_stdlib
                hardware_timer
                hardware_dma
                tinyusb_device 
                tinyusb_board
                logic_analyser
)

pico_add_extra_outputs(usbtmc)

##################################################################

project(vxitmc C CXX)


# Add executable. Default name is the project name, version 0.1
add_executable(vxitmc 
        main_vxi.c
        commands.c
        rpc_server.c
        vxi_core_prog.c
        eeprom_24lc02b.c
)

target_include_directories(vxitmc PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_set_program_name(vxitmc "vxitmc")
pico_set_program_version(vxitmc "0.1")

pico_enable_stdio_uart(vxitmc 1)
pico_enable_stdio_usb(vxitmc 0)

target_link_libraries(vxitmc 
                pico_stdlib
                hardware_timer
                hardware_dma
                hardware_i2c
                tinyusb_device 
                tinyusb_board
                logic_analyser
                pico_cyw43_arch_lwip_poll
                pico_lwip_mdns
)
pico_add_extra_outputs(vxitmc)
