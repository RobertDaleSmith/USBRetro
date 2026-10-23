// seesaw.h - Adafruit Seesaw I2C Protocol Driver
//
// Generic driver for Adafruit seesaw devices (ATtiny8x7-based).
// Supports GPIO bulk read, ADC read, and pin configuration.

#ifndef SEESAW_H
#define SEESAW_H

#include <stdint.h>
#include <stdbool.h>
#include "platform/platform_i2c.h"

// Default seesaw I2C address
#define SEESAW_ADDR_DEFAULT 0x49

// Seesaw module registers
#define SEESAW_STATUS_BASE   0x00
#define SEESAW_GPIO_BASE     0x01
#define SEESAW_ADC_BASE      0x09

// Status functions
#define SEESAW_STATUS_HW_ID  0x01
#define SEESAW_STATUS_SWRST  0x7F

// GPIO functions
#define SEESAW_GPIO_DIRCLR   0x03
#define SEESAW_GPIO_BULK     0x04
#define SEESAW_GPIO_BULK_SET 0x05
#define SEESAW_GPIO_PULLENSET 0x0B

#define SEESAW_HW_ID_SAMD09    0x55
#define SEESAW_HW_ID_TINY416   0x46
#define SEESAW_HW_ID_TINY806   0x84
#define SEESAW_HW_ID_TINY807   0x85
#define SEESAW_HW_ID_TINY816   0x86
#define SEESAW_HW_ID_TINY817   0x87
#define SEESAW_HW_ID_TINY1616  0x88
#define SEESAW_HW_ID_TINY1617  0x89
#define SEESAW_HW_ID_STM32C011 0x90
#define SEESAW_HW_ID_STM32C031 0x91
#define SEESAW_HW_ID_STM32C051 0x92
#define SEESAW_HW_ID_STM32G071 0x93

#define SEESAW_BEGIN_ATTEMPTS  10
#define SEESAW_BEGIN_RETRY_MS  10

// ADC functions
#define SEESAW_ADC_CHANNEL_OFFSET 0x07
#define SEESAW_ADC_ERROR 0xFFFF  // Returned on I2C read failure (valid range 0-1023)

// Seesaw device handle
typedef struct {
    platform_i2c_t bus;
    uint8_t addr;
} seesaw_device_t;

// Initialize a seesaw device
void seesaw_init(seesaw_device_t* dev, platform_i2c_t bus, uint8_t addr);

// Configure GPIO pins as inputs with pull-ups
// pin_mask: bitmask of pins to configure (e.g. (1<<6)|(1<<7) for pins 6,7)
bool seesaw_gpio_set_input_pullup(seesaw_device_t* dev, uint32_t pin_mask);

// Read all GPIO pins as a 32-bit bitmask (1 = high, 0 = low)
uint32_t seesaw_gpio_read_bulk(seesaw_device_t* dev);

// Read a single ADC channel (returns 10-bit value, 0-1023)
uint16_t seesaw_adc_read(seesaw_device_t* dev, uint8_t channel);

// Software reset the seesaw
bool seesaw_reset(seesaw_device_t* dev);

// Read hardware ID (expected: 0x87 for ATtiny8x7)
uint8_t seesaw_get_hw_id(seesaw_device_t* dev);

uint8_t seesaw_begin(seesaw_device_t* dev);

#endif // SEESAW_H
