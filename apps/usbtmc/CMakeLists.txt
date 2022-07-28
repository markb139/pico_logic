project(usbtmc C CXX)

# Add executable. Default name is the project name, version 0.1
add_executable(usbtmc 
        main.c 
        usb_descriptors.c 
        usbtmc_app.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../commands.c
)

target_include_directories(usbtmc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

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
