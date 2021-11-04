cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(pcfxmouse C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

# Initialize the SDK
pico_sdk_init()

set(FAMILY rp2040)

add_executable(pcfxmouse)

pico_add_extra_outputs(pcfxmouse)

pico_generate_pio_header(pcfxmouse ${CMAKE_CURRENT_LIST_DIR}/pcfxplex.pio )
pico_generate_pio_header(pcfxmouse ${CMAKE_CURRENT_LIST_DIR}/clock.pio )


# Example source
target_sources(pcfxmouse PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/hid_app.c
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/msc_app.c
        )

# Example include
target_include_directories(pcfxmouse PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(pcfxmouse PRIVATE
	pico_stdlib
	pico_multicore
	hardware_pio
	tinyusb_host
	tinyusb_board
	)

