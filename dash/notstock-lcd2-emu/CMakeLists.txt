# NOT STOCK 2" gauge (RP2350-Touch-LCD-2), ECUMaster EMU Black.
#   cmake -S . -B build -DPICO_SDK_PATH=/path/to/pico-sdk -DLVGL_DIR=/path/to/lvgl-8.4
#   cmake --build build -j
# -> build/notstock_lcd2_emu.uf2
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")   # RP2350A, 16 MB flash below
include(pico_sdk_import.cmake)
project(notstock_lcd2_emu C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(LVGL_DIR "" CACHE PATH "LVGL 8.4 source tree")
if(NOT LVGL_DIR)
    message(FATAL_ERROR "set -DLVGL_DIR=/path/to/lvgl (8.4)")
endif()
set(EMU_DIR ${CMAKE_CURRENT_LIST_DIR}/../notstock-round-emu/main)

file(GLOB_RECURSE LVGL_SRC ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SRC})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${LVGL_DIR}/src
                           ${CMAKE_CURRENT_LIST_DIR}/main)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -O2 -w)
target_link_libraries(lvgl pico_stdlib)

file(GLOB FONTS ${CMAKE_CURRENT_LIST_DIR}/main/fonts/*.c)
add_executable(notstock_lcd2_emu
    main/main.c
    main/hw_l2.c
    main/can_l2.c
    main/ui_l2.c
    main/l2_bg.c
    main/l2_splash.c
    ${FONTS}
    ${CMAKE_CURRENT_BINARY_DIR}/can_ram.o
)
target_include_directories(notstock_lcd2_emu PRIVATE
    main ${EMU_DIR} third_party/can2040)

# can2040 and the stream decoder run in the PIO interrupt: into RAM, so no
# flash access delays it (can2040 docs/API.md, "Low interrupt latency").
# Compiled without per-function sections, then .text / .rodata renamed to
# .time_critical.*, which the SDK's linker script copies to RAM.
add_library(can_ram_src OBJECT third_party/can2040/can2040.c ${EMU_DIR}/emu_stream.c)
target_include_directories(can_ram_src PRIVATE ${EMU_DIR} third_party/can2040)
target_compile_options(can_ram_src PRIVATE -O2 -fno-function-sections -fno-data-sections)
# headers only: the SDK sources are linked once, into the executable
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
# 16 MB W25Q128 on the board
target_compile_definitions(notstock_lcd2_emu PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216)
target_link_libraries(notstock_lcd2_emu
    pico_stdlib hardware_spi hardware_i2c hardware_pwm hardware_dma
    hardware_pio hardware_irq hardware_clocks lvgl)
pico_enable_stdio_usb(notstock_lcd2_emu 1)
pico_enable_stdio_uart(notstock_lcd2_emu 0)
pico_add_extra_outputs(notstock_lcd2_emu)
