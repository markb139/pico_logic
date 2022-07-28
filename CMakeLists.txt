# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-logic C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD "pico_w")

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/pi/pico/pico-sdk")
# Initialise the Pico SDK
pico_sdk_init()


# add the analyser library
add_subdirectory(libs)
# add the apps
add_subdirectory(apps)
