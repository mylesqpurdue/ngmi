cmake_minimum_required(VERSION 3.13)

add_executable(myles_board
    main.c
    display.c
    waves.c
    inputs.c
    led.c
    game.c
    audio.c
)

target_include_directories(myles_board PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(myles_board
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_dma
    hardware_adc
    hardware_pwm
    hardware_timer
    pico_rand
)

# Enable USB serial output for debugging
pico_enable_stdio_usb(myles_board 1)
pico_enable_stdio_uart(myles_board 0)

# Generate .uf2 and .hex for flashing
pico_add_extra_outputs(myles_board)
