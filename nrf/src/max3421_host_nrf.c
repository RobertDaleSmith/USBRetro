// max3421_host_nrf.c - MAX3421E USB Host for nRF52840 (Zephyr)
//
// Implements TinyUSB MAX3421E platform callbacks using direct nRF SPI registers.
// Used with the Adafruit MAX3421E FeatherWing (#5858) on Feather nRF52840.
//
// Uses direct NRF_SPI1 register access instead of Zephyr's SPI driver because
// TinyUSB calls tuh_max3421_spi_xfer_api() from ISR context (via hcd_int_handler),
// and Zephyr's SPI driver uses k_sem_take(K_FOREVER) which is not ISR-safe.
//
// FeatherWing pin mapping (Feather nRF52840):
//   SPI1: SCK=P0.14, MOSI=P0.13, MISO=P0.15
//   CS:   D10 = P0.27
//   IRQ:  D9  = P0.26 (active low, falling edge)

#include "tusb.h"

#if CFG_TUH_MAX3421

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <nrfx.h>
#include <stdio.h>

// ============================================================================
// PIN DEFINITIONS (Feather nRF52840 + MAX3421E FeatherWing)
// ============================================================================

#define SPI_PERIPH   NRF_SPI1

#define SPI_SCK_PIN  14  // P0.14
#define SPI_MOSI_PIN 13  // P0.13
#define SPI_MISO_PIN 15  // P0.15

#define CS_PIN       27  // P0.27 (D10)
#define INT_PIN      26  // P0.26 (D9, active low)

// ============================================================================
// STATE
// ============================================================================

static const struct device *int_gpio_dev;
static struct gpio_callback int_cb_data;
static volatile bool int_enabled = false;
static volatile bool in_handler = false;  // re-entrancy guard

// Debug counters
static volatile uint32_t isr_count = 0;
static volatile uint32_t int_api_en = 0;
static volatile uint32_t int_api_dis = 0;
static volatile uint32_t missed_edge_count = 0;

// Detection status
static bool max3421_detected = false;
static uint8_t max3421_revision = 0;

bool max3421_is_detected(void) { return max3421_detected; }
uint8_t max3421_get_revision(void) { return max3421_revision; }

// ============================================================================
// DIRECT SPI (ISR-safe, no RTOS dependencies)
// ============================================================================

static void spi_direct_init(void)
{
    SPI_PERIPH->ENABLE = SPI_ENABLE_ENABLE_Disabled;
    SPI_PERIPH->PSEL.SCK  = SPI_SCK_PIN;
    SPI_PERIPH->PSEL.MOSI = SPI_MOSI_PIN;
    SPI_PERIPH->PSEL.MISO = SPI_MISO_PIN;
    SPI_PERIPH->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) |
                         (SPI_CONFIG_CPHA_Leading   << SPI_CONFIG_CPHA_Pos)  |
                         (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);
    SPI_PERIPH->FREQUENCY = SPI_FREQUENCY_FREQUENCY_M8;
    SPI_PERIPH->ENABLE = SPI_ENABLE_ENABLE_Enabled;
}

static void spi_direct_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        SPI_PERIPH->EVENTS_READY = 0;
        SPI_PERIPH->TXD = tx ? tx[i] : 0x00;
        while (!SPI_PERIPH->EVENTS_READY) {}
        uint8_t b = (uint8_t)SPI_PERIPH->RXD;
        if (rx) rx[i] = b;
    }
}

// ============================================================================
// CS CONTROL (direct GPIO register, ISR-safe)
// ============================================================================

static inline void cs_assert(void)   { NRF_P0->OUTCLR = (1U << CS_PIN); }
static inline void cs_deassert(void)  { NRF_P0->OUTSET = (1U << CS_PIN); }

// ============================================================================
// TinyUSB PLATFORM CALLBACKS
// ============================================================================

void tuh_max3421_spi_cs_api(uint8_t rhport, bool active)
{
    (void)rhport;
    if (active) cs_assert(); else cs_deassert();
}

bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf,
                               uint8_t *rx_buf, size_t xfer_bytes)
{
    (void)rhport;
    spi_direct_xfer(tx_buf, rx_buf, xfer_bytes);
    return true;
}

// INT enable/disable — TinyUSB calls this to gate interrupts during SPI xfer.
// When re-enabling, checks for missed edge (INT already active = no falling edge).
// Re-entrancy guard prevents infinite recursion from spi_unlock→int_api→hcd_int_handler→spi_lock/unlock→int_api.
void tuh_max3421_int_api(uint8_t rhport, bool enabled)
{
    (void)rhport;
    if (enabled && !int_enabled) {
        int_api_en++;
        gpio_pin_interrupt_configure(int_gpio_dev, INT_PIN,
                                     GPIO_INT_EDGE_TO_ACTIVE);
        int_enabled = true;

        // Missed-edge check: if INT is already active, no falling edge will
        // occur. Call hcd_int_handler directly, with re-entrancy guard.
        if (!in_handler && gpio_pin_get(int_gpio_dev, INT_PIN)) {
            missed_edge_count++;
            in_handler = true;
            hcd_int_handler(1, false);
            in_handler = false;
        }
    } else if (!enabled && int_enabled) {
        int_api_dis++;
        gpio_pin_interrupt_configure(int_gpio_dev, INT_PIN,
                                     GPIO_INT_DISABLE);
        int_enabled = false;
    }
}

// ============================================================================
// GPIO INTERRUPT HANDLER
// ============================================================================

static void max3421_int_isr(const struct device *dev, struct gpio_callback *cb,
                             uint32_t pins)
{
    (void)dev; (void)cb; (void)pins;
    isr_count++;
    hcd_int_handler(1, true);
}

// ============================================================================
// SPI REGISTER ACCESS (for probe only — before interrupts are enabled)
// ============================================================================

static void max3421_reg_write(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)((reg << 3) | 0x02), val };
    cs_assert();
    spi_direct_xfer(tx, NULL, 2);
    cs_deassert();
}

static uint8_t max3421_reg_read(uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(reg << 3), 0x00 };
    uint8_t rx[2] = { 0, 0 };
    cs_assert();
    spi_direct_xfer(tx, rx, 2);
    cs_deassert();
    return rx[1];
}

#define MAX3421_REG_PINCTL   17
#define MAX3421_REG_REVISION 18
#define MAX3421_FDUPSPI      0x10

static bool max3421_probe(void)
{
    max3421_reg_write(MAX3421_REG_PINCTL, MAX3421_FDUPSPI);

    uint8_t r1 = max3421_reg_read(MAX3421_REG_REVISION);
    uint8_t r2 = max3421_reg_read(MAX3421_REG_REVISION);
    uint8_t r3 = max3421_reg_read(MAX3421_REG_REVISION);

    printf("[max3421] Probe: rev 0x%02X 0x%02X 0x%02X\n", r1, r2, r3);

    if (r1 != r2 || r2 != r3) return false;
    if ((r1 & 0xF0) != 0x10) return false;

    return true;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool max3421_host_init(void)
{
    printf("[max3421] Init SPI host\n");

    spi_direct_init();

    // CS pin: output, deasserted (high)
    NRF_P0->PIN_CNF[CS_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                               (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                               (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                               (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos);
    cs_deassert();

    // INT pin: input with pull-up, active low
    int_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(int_gpio_dev)) {
        printf("[max3421] ERROR: GPIO not ready\n");
        return false;
    }
    gpio_pin_configure(int_gpio_dev, INT_PIN,
                       GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_LOW);

    if (!max3421_probe()) {
        printf("[max3421] ERROR: chip not detected\n");
        max3421_detected = false;
        return false;
    }

    max3421_revision = max3421_reg_read(MAX3421_REG_REVISION);
    printf("[max3421] Chip rev 0x%02X\n", max3421_revision);
    max3421_detected = true;

    // Set up GPIO interrupt callback (but don't enable yet)
    gpio_init_callback(&int_cb_data, max3421_int_isr, BIT(INT_PIN));
    gpio_add_callback(int_gpio_dev, &int_cb_data);

    return true;
}

void max3421_host_enable_int(void)
{
    // No chip: never arm the IRQ. The pin would be left floating-ish on a pull-up
    // with nothing driving it, and any edge on it would call hcd_int_handler()
    // against a host stack that was never tusb_init()'d. This is reachable
    // because the board builds with the host compiled in whether or not the
    // FeatherWing is fitted — see usbh_init()'s probe-then-init sequence.
    if (!max3421_detected) {
        printf("[max3421] INT not armed (chip not detected)\n");
        return;
    }

    gpio_pin_interrupt_configure(int_gpio_dev, INT_PIN,
                                 GPIO_INT_EDGE_TO_ACTIVE);
    int_enabled = true;

    int raw = gpio_pin_get(int_gpio_dev, INT_PIN);
    printf("[max3421] INT on, pin=%d\n", raw);

    // If INT already active, trigger handler (missed edge)
    if (raw) {
        hcd_int_handler(1, false);
    }
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

void max3421_get_diag(uint8_t *out_hirq, uint8_t *out_mode,
                      uint8_t *out_hrsl, uint8_t *out_int_pin)
{
    if (!max3421_detected) {
        *out_hirq = 0; *out_mode = 0; *out_hrsl = 0; *out_int_pin = 0;
        return;
    }
    // Disable ALL interrupts during SPI reads to prevent bus contention
    unsigned int key = irq_lock();
    *out_hirq = max3421_reg_read(25);   // HIRQ
    *out_mode = max3421_reg_read(27);   // MODE
    *out_hrsl = max3421_reg_read(31);   // HRSL
    *out_int_pin = gpio_pin_get(int_gpio_dev, INT_PIN) ? 1 : 0;
    irq_unlock(key);
}

void max3421_print_diag(void)
{
    int pin = gpio_pin_get(int_gpio_dev, INT_PIN);
    printf("[m] isr=%u me=%u pin=%d\n",
           (unsigned)isr_count, (unsigned)missed_edge_count, pin);
}

#endif // CFG_TUH_MAX3421
