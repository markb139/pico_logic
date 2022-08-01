##################################################################

project(vxitmc C CXX)

# Add executable. Default name is the project name, version 0.1
add_executable(vxitmc 
        main_vxi.c
        rpc_server.c
        vxi_core_prog.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../commands.c
)

add_compile_definitions(PICO_DEFAULT_UART_TX_PIN=16)
add_compile_definitions(PICO_DEFAULT_UART_RX_PIN=17)


if (ENABLE_EEPROM)
        message("ENABLE_EEPROM")
        add_compile_definitions(ENABLE_EEPROM=TRUE)
        target_sources(vxitmc
                PUBLIC eeprom_24lc02b.c
        )
elseif(WIFISSID AND WIFIPWD)
        message("WIFI")
        add_compile_definitions(WIFI_SSID=WIFISSID)
        add_compile_definitions(WIFI_PWD=WIFIPWD)
else()
        message(FATAL_ERROR "WiFi not possible")
endif()

target_include_directories(vxitmc PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR} 
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/..
)

pico_set_program_name(vxitmc "vxitmc")
pico_set_program_version(vxitmc "0.1")

pico_enable_stdio_uart(vxitmc 1)
pico_enable_stdio_usb(vxitmc 0)

target_link_libraries(vxitmc 
                pico_stdlib
                hardware_timer
                hardware_dma
                hardware_i2c
                logic_analyser
                pico_cyw43_arch_lwip_poll
                pico_lwip_mdns
)
pico_add_extra_outputs(vxitmc)
