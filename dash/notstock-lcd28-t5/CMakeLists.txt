# NOT STOCK 2.8" gauge (RP2350-Touch-LCD-2.8), VW T5.1 over OBD-II.
#   cmake -S . -B build -DPICO_SDK_PATH=/path/to/pico-sdk -DLVGL_DIR=/path/to/lvgl-8.4
#   cmake --build build -j
# -> build/notstock_lcd28_t5.uf2
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")   # RP2350A, 16 MB flash below
include(pico_sdk_import.cmake)
project(notstock_lcd28_t5 C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(LVGL_DIR "" CACHE PATH "LVGL 8.4 source tree")
if(NOT LVGL_DIR)
    message(FATAL_ERROR "set -DLVGL_DIR=/path/to/lvgl (8.4)")
endif()
# shared: the 2" gauge's can2040; the 7" dash's OBD-II / UDS
# client and trouble code texts
set(L2_DIR ${CMAKE_CURRENT_LIST_DIR}/../notstock-lcd2-emu)
set(DASH7_DIR ${CMAKE_CURRENT_LIST_DIR}/../notstock-dash-7inch/main)
set(CAN2040_DIR ${L2_DIR}/third_party/can2040)

file(GLOB_RECURSE LVGL_SRC ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SRC})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${LVGL_DIR}/src
                           ${CMAKE_CURRENT_LIST_DIR}/main)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -O2 -w)
target_link_libraries(lvgl pico_stdlib)

file(GLOB FONTS ${CMAKE_CURRENT_LIST_DIR}/main/fonts/*.c)
add_executable(notstock_lcd28_t5
    main/main.c
    main/hw_l28.c
    main/can_t5.c
    main/ui_t5.c
    main/audio_l28.c
    main/faces/faces28.c
    ${DASH7_DIR}/obd2.c
    ${DASH7_DIR}/dtc_text.c
    main/t5_splash.c
    ${FONTS}
    ${CMAKE_CURRENT_BINARY_DIR}/can_ram.o
)
# obd2.c's rusefi_can.h: only the dash_data_t it decodes into
target_include_directories(notstock_lcd28_t5 PRIVATE
    main main/faces ${DASH7_DIR} ${CAN2040_DIR})
pico_generate_pio_header(notstock_lcd28_t5 ${CMAKE_CURRENT_LIST_DIR}/main/i2s_out.pio)

# can2040 runs in the PIO interrupt: into RAM, so no flash access delays it
# (can2040 docs/API.md, "Low interrupt latency"). Compiled without
# per-function sections, then .text / .rodata renamed to .time_critical.*,
# which the SDK's linker script copies to RAM.
add_library(can_ram_src OBJECT ${CAN2040_DIR}/can2040.c)
target_include_directories(can_ram_src PRIVATE ${CAN2040_DIR})
target_compile_options(can_ram_src PRIVATE -O2 -fno-function-sections -fno-data-sections)
target_link_libraries(can_ram_src PRIVATE pico_stdlib_headers hardware_pio_headers
    hardware_dma_headers hardware_irq_headers)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/can_ram.o
    COMMAND ${CMAKE_LINKER} -r -o ${CMAKE_CURRENT_BINARY_DIR}/can_ram_pre.o $<TARGET_OBJECTS:can_ram_src>
    COMMAND ${CMAKE_OBJCOPY}
        --rename-section .text=.time_critical.can,alloc,load,readonly,code,contents
        --rename-section .rodata=.time_critical.can_ro,alloc,load,readonly,data,contents
        ${CMAKE_CURRENT_BINARY_DIR}/can_ram_pre.o ${CMAKE_CURRENT_BINARY_DIR}/can_ram.o
    DEPENDS $<TARGET_OBJECTS:can_ram_src>
    COMMAND_EXPAND_LISTS
    VERBATIM)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/can_ram.o PROPERTIES
    EXTERNAL_OBJECT TRUE GENERATED TRUE)
# 16 MB on the board
target_compile_definitions(notstock_lcd28_t5 PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216)
target_link_libraries(notstock_lcd28_t5
    pico_stdlib hardware_spi hardware_i2c hardware_pwm hardware_dma
    hardware_pio hardware_irq hardware_clocks hardware_flash lvgl)
pico_enable_stdio_usb(notstock_lcd28_t5 1)
pico_enable_stdio_uart(notstock_lcd28_t5 0)
pico_add_extra_outputs(notstock_lcd28_t5)
