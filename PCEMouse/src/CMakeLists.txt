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
add_executable(usbretro_xb1) # For the usbretro_xb1 target
add_executable(usbretro_nuon) # For the usbretro_nuon target

target_compile_definitions(usbretro_pce PRIVATE CONFIG_PCE=1)
target_compile_definitions(usbretro_ngc PRIVATE CONFIG_NGC=1)
target_compile_definitions(usbretro_xb1 PRIVATE CONFIG_XB1=1)
target_compile_definitions(usbretro_nuon PRIVATE CONFIG_NUON=1)

pico_add_extra_outputs(usbretro_pce)
pico_add_extra_outputs(usbretro_ngc)
pico_add_extra_outputs(usbretro_xb1)
pico_add_extra_outputs(usbretro_nuon)

pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/plex.pio )
pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/clock.pio )
pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/select.pio )
pico_generate_pio_header(usbretro_pce ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio )
pico_generate_pio_header(usbretro_ngc ${CMAKE_CURRENT_LIST_DIR}/lib/joybus-pio/src/joybus.pio )
pico_generate_pio_header(usbretro_ngc ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio )
pico_generate_pio_header(usbretro_xb1 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio )
pico_generate_pio_header(usbretro_nuon ${CMAKE_CURRENT_LIST_DIR}/polyface_read.pio )
pico_generate_pio_header(usbretro_nuon ${CMAKE_CURRENT_LIST_DIR}/polyface_send.pio )
pico_generate_pio_header(usbretro_nuon ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio )

## SET TARGE SOURCES
set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.c
    ${CMAKE_CURRENT_SOURCE_DIR}/xinput_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/sony_ds3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/sony_ds4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/sony_ds5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/sony_psc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/8bitdo_pce.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/device_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/device_registry.c
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

target_sources(usbretro_xb1 PUBLIC # For XB1
    ${COMMON_SOURCES}
)

target_sources(usbretro_nuon PUBLIC # For NUON
    ${COMMON_SOURCES}
)

## SET TARGET INCLUDE DIRS
target_include_directories(usbretro_pce PUBLIC # FOR PCE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(usbretro_ngc PUBLIC # FOR NGC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/joybus-pio/include
)

target_include_directories(usbretro_xb1 PUBLIC # FOR XB1
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(usbretro_nuon PUBLIC # FOR NUON
    ${CMAKE_CURRENT_SOURCE_DIR}
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

target_link_libraries(usbretro_xb1 PRIVATE # FOR XB1
    ${COMMON_LIBRARIES}
    pico_i2c_slave
    hardware_i2c
)

target_link_libraries(usbretro_nuon PRIVATE # FOR NUON
    ${COMMON_LIBRARIES}
    pico_bit_ops
)
