cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(usbretro C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

# Initialize the SDK
pico_sdk_init()

set(FAMILY rp2040)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/tusb_xinput xinput_host)

add_executable(usbretro_pce) # For the usbretro_pce target
add_executable(usbretro_ngc) # For the usbretro_ngc target

target_compile_definitions(usbretro_pce PRIVATE CONFIG_PCE=1)
target_compile_definitions(usbretro_ngc PRIVATE CONFIG_NGC=1)

pico_add_extra_outputs(usbretro_pce)
pico_add_extra_outputs(usbretro_ngc)

pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/plex.pio )
pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/clock.pio )
pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/select.pio )
pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio )
pico_generate_pio_header(usbretro_ngc ${CMAKE_CURRENT_LIST_DIR}/lib/joybus-pio/src/joybus.pio )

## SET TARGE SOURCES
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.c
    ${CMAKE_CURRENT_SOURCE_DIR}/xinput_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

target_sources(usbretro_pce PUBLIC # For PCE
    ${COMMON_SOURCES}
)

target_sources(usbretro_ngc PUBLIC # For NGC
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/joybus-pio/src/joybus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/joybus-pio/src/GamecubeConsole.c
)

## SET TARGET INCLUDE DIRS
target_include_directories(usbretro_pce PUBLIC # FOR PCE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(usbretro_ngc PUBLIC # FOR NGC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/joybus-pio/include
)

## SET TARGET LINK LIBS
set(COMMON_LIBRARIES
    pico_stdlib
    pico_multicore
    hardware_pio
    tinyusb_host
    tinyusb_board
    xinput_host_app_driver
)

target_link_libraries(usbretro_pce PRIVATE # FOR PCE
    ${COMMON_LIBRARIES}
)

target_link_libraries(usbretro_ngc PRIVATE # FOR NGC
    ${COMMON_LIBRARIES}
)
