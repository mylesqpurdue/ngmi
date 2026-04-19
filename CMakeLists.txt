cmake_minimum_required(VERSION 3.13)

# Pico SDK bootstrap — set PICO_SDK_PATH in your environment or pass via -DPICO_SDK_PATH=...
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ngmi_bomb_game C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ─── Team module sub-projects ─────────────────────────────────────────────────
add_subdirectory(alexia)
add_subdirectory(myles)   # FFT Wave + Sound
# add_subdirectory(celine)  # Button + Wires   — uncomment when added
# add_subdirectory(krish)   # Morse + Ring of Fire — uncomment when added
