set(COMMON_SOURCE commands.c)

add_compile_definitions(ANALYSER_PIN_BASE=0)
add_compile_definitions(GENERATOR_PIN_BASE=8)

add_subdirectory(usbtmc)
add_subdirectory(vxitmc)