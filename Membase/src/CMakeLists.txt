cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(membase C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(membase)

pico_add_extra_outputs(membase)

pico_generate_pio_header(membase ${CMAKE_CURRENT_LIST_DIR}/membase.pio)

target_sources(membase PRIVATE membase.c)

target_link_libraries(membase PRIVATE
        pico_stdlib
        pico_multicore
        hardware_pio
        hardware_flash
        )

# Don't execute from Flash; keep it pinned in SRAM
#
pico_set_binary_type(membase copy_to_ram)

