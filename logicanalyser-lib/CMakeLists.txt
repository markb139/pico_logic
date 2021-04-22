###########################################################################################
#   Logic analyser capture library
#   Mark Burton 2021
#
#   Features:
#       logic capture
#       logic generation
###########################################################################################


add_library(logic_analyser INTERFACE)

pico_generate_pio_header(logic_analyser ${CMAKE_CURRENT_LIST_DIR}/logic_analyser.pio)

target_sources(logic_analyser INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/rp2040-logic-analyzer.c
    ${CMAKE_CURRENT_LIST_DIR}/rp2040-logic-generator.c
)

target_link_libraries(logic_analyser INTERFACE
    pico_stdlib
    hardware_pio 
    hardware_dma
)
