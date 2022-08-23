cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(pcemouse C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

# Initialize the SDK
pico_sdk_init()

set(FAMILY rp2040)

add_executable(pcemouse)

pico_add_extra_outputs(pcemouse)

pico_generate_pio_header(pcemouse ${CMAKE_CURRENT_LIST_DIR}/plex.pio )
pico_generate_pio_header(pcemouse ${CMAKE_CURRENT_LIST_DIR}/clock.pio )
pico_generate_pio_header(pcemouse ${CMAKE_CURRENT_LIST_DIR}/select.pio )
pico_generate_pio_header(pcemouse ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio )


# Example source
target_sources(pcemouse PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.c
        ${CMAKE_CURRENT_SOURCE_DIR}/xinput_host.c
        ${CMAKE_CURRENT_SOURCE_DIR}/xinput_app.c
        ${CMAKE_CURRENT_SOURCE_DIR}/hid_app.c
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/msc_app.c
        )

# Example include
target_include_directories(pcemouse PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(pcemouse PRIVATE
	pico_stdlib
	pico_multicore
	hardware_pio
	tinyusb_host
	tinyusb_board
	)

