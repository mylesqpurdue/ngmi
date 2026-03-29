cmake_minimum_required(VERSION 3.13)

add_executable(alexia_board
    main.c
    simon_says.c
    timer_module.c
    tm1637.c
)

target_include_directories(alexia_board PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(alexia_board
    pico_stdlib
    hardware_gpio
    hardware_uart
    hardware_timer
)

# Enable USB serial output for debugging (optional — remove for final build)
pico_enable_stdio_usb(alexia_board 1)
pico_enable_stdio_uart(alexia_board 0)

# Generate .uf2 and .hex for flashing
pico_add_extra_outputs(alexia_board)
