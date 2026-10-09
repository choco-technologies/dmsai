#include "dmsai_port.h"
#include "dmclk_port.h"
#include "dmod.h"
#include <errno.h>

#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844U)
#define RCC_CR      (*(volatile uint32_t *)0x40023800U)
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830U)
#define SAI2_ACR1   (*(volatile uint32_t *)0x40015C04U)
#define SAI2_BCR1   (*(volatile uint32_t *)0x40015C24U)
#define SAI2_AFRCR  (*(volatile uint32_t *)0x40015C0CU)
#define SAI2_ASLOTR (*(volatile uint32_t *)0x40015C10U)
#define SAI2_ASR    (*(volatile uint32_t *)0x40015C18U)
#define SAI2_BSR    (*(volatile uint32_t *)0x40015C38U)
#define SAI2EN (1U << 23)
#define DMA2_S4CR   (*(volatile uint32_t *)0x40026470U)
#define DMA2_S4NDTR (*(volatile uint32_t *)0x40026474U)
#define DMA2_S6CR   (*(volatile uint32_t *)0x400264A0U)
#define DMA2_S6NDTR (*(volatile uint32_t *)0x400264A4U)
#define GPIOG_BASE 0x40021800U
#define GPIOI_BASE 0x40022000U

typedef struct {
    uint32_t ahb1enr;
    uint32_t g_moder, g_otyper, g_ospeedr, g_pupdr, g_afrh;
    uint32_t i_moder, i_otyper, i_ospeedr, i_pupdr, i_afrl;
} pin_backup_t;

/** @brief Access a GPIO register on the board's SAI2 pin banks. */
static volatile uint32_t *gpio_reg(uintptr_t base, uint32_t offset)
{
    return (volatile uint32_t *)(base + offset);
}

/** @brief Route the Discovery board's SAI2 signals through AF10. */
static void setup_sai2_pins(pin_backup_t *backup)
{
    backup->ahb1enr = RCC_AHB1ENR;
    RCC_AHB1ENR |= (1U << 6) | (1U << 8);
    backup->g_moder = *gpio_reg(GPIOG_BASE, 0);
    backup->g_otyper = *gpio_reg(GPIOG_BASE, 4);
    backup->g_ospeedr = *gpio_reg(GPIOG_BASE, 8);
    backup->g_pupdr = *gpio_reg(GPIOG_BASE, 12);
    backup->g_afrh = *gpio_reg(GPIOG_BASE, 36);
    backup->i_moder = *gpio_reg(GPIOI_BASE, 0);
    backup->i_otyper = *gpio_reg(GPIOI_BASE, 4);
    backup->i_ospeedr = *gpio_reg(GPIOI_BASE, 8);
    backup->i_pupdr = *gpio_reg(GPIOI_BASE, 12);
    backup->i_afrl = *gpio_reg(GPIOI_BASE, 32);
    for (unsigned pin = 4; pin <= 7; ++pin) {
        *gpio_reg(GPIOI_BASE, 0) =
            (*gpio_reg(GPIOI_BASE, 0) & ~(3U << (pin * 2))) |
            (2U << (pin * 2));
        *gpio_reg(GPIOI_BASE, 4) &= ~(1U << pin);
        *gpio_reg(GPIOI_BASE, 8) |= 3U << (pin * 2);
        *gpio_reg(GPIOI_BASE, 12) &= ~(3U << (pin * 2));
        *gpio_reg(GPIOI_BASE, 32) =
            (*gpio_reg(GPIOI_BASE, 32) & ~(15U << (pin * 4))) |
            (10U << (pin * 4));
    }
    *gpio_reg(GPIOG_BASE, 0) =
        (*gpio_reg(GPIOG_BASE, 0) & ~(3U << 20)) | (2U << 20);
    *gpio_reg(GPIOG_BASE, 4) &= ~(1U << 10);
    *gpio_reg(GPIOG_BASE, 8) |= 3U << 20;
    *gpio_reg(GPIOG_BASE, 12) &= ~(3U << 20);
    *gpio_reg(GPIOG_BASE, 36) =
        (*gpio_reg(GPIOG_BASE, 36) & ~(15U << 8)) | (10U << 8);
}

/** @brief Return board GPIO registers to their pretest state. */
static void restore_sai2_pins(const pin_backup_t *backup)
{
    *gpio_reg(GPIOG_BASE, 0) = backup->g_moder;
    *gpio_reg(GPIOG_BASE, 4) = backup->g_otyper;
    *gpio_reg(GPIOG_BASE, 8) = backup->g_ospeedr;
    *gpio_reg(GPIOG_BASE, 12) = backup->g_pupdr;
    *gpio_reg(GPIOG_BASE, 36) = backup->g_afrh;
    *gpio_reg(GPIOI_BASE, 0) = backup->i_moder;
    *gpio_reg(GPIOI_BASE, 4) = backup->i_otyper;
    *gpio_reg(GPIOI_BASE, 8) = backup->i_ospeedr;
    *gpio_reg(GPIOI_BASE, 12) = backup->i_pupdr;
    *gpio_reg(GPIOI_BASE, 32) = backup->i_afrl;
    RCC_AHB1ENR = backup->ahb1enr;
}

static int failures;
#define CHECK(test) do { if (!(test)) { \
    Dmod_Printf("DMSAI PORT FAIL line %d: %s\n", __LINE__, #test); \
    ++failures; \
} } while (0)

/** @brief Configure, transfer on and release SAI2 at one sample rate. */
static void exercise(uint32_t rate)
{
    dmsai_config_t config = {
        .instance = 1, .clock_role = dmsai_clock_master,
        .framing = dmsai_frame_tdm, .pcm_format = dmsai_pcm_s16_le,
        .sample_rate_hz = rate, .tolerance_ppm = 500,
        .slot_bits = 16, .slot_count = 4, .frame_bits = 64,
        .active_slots = 5, .transmit = true, .receive = true,
    };
    uint32_t before_cr = RCC_CR;
    dmclk_frequency_t before_sai =
        dmclk_port_get_domain_frequency(dmclk_domain_sai2);
    int rc = dmsai_port_init(&config);
    Dmod_Printf("DMSAI SAI2 init rate=%u rc=%d\n", (unsigned)rate, rc);
    CHECK(rc == 0);
    if (rc != 0) return;
    CHECK(dmsai_port_init(&config) == -EBUSY);
    CHECK((RCC_APB2ENR & SAI2EN) != 0);
    CHECK((SAI2_ACR1 & (15U << 20)) == (2U << 20));
    CHECK((SAI2_BCR1 & (3U << 10)) == (1U << 10));
    CHECK(SAI2_AFRCR == 0x00051F3FU);
    CHECK(SAI2_ASLOTR == 0x00050340U);
    dmsai_status_t status;
    CHECK(dmsai_port_get_status(1, &status) == 0);
    uint32_t error = status.actual_sample_rate_hz > rate ?
        status.actual_sample_rate_hz - rate : rate - status.actual_sample_rate_hz;
    CHECK(error <= rate / 2000U);
    CHECK(dmsai_port_start(1) == 0);
    CHECK((SAI2_ACR1 & (1U << 16)) != 0);
    CHECK((SAI2_BCR1 & (1U << 16)) != 0);
    CHECK((SAI2_ACR1 & (1U << 17)) != 0);
    CHECK((SAI2_BCR1 & (1U << 17)) != 0);
    CHECK((DMA2_S4CR & 0x101U) == 0x101U);
    CHECK((DMA2_S6CR & 0x101U) == 0x101U);
    uint32_t tx_before = DMA2_S4NDTR;
    uint32_t rx_before = DMA2_S6NDTR;
    bool tx_progress = false;
    bool rx_progress = false;
    for (unsigned i = 0; i < 10; ++i) {
        Dmod_ThreadSleep(1);
        if (DMA2_S4NDTR != tx_before) tx_progress = true;
        if (DMA2_S6NDTR != rx_before) rx_progress = true;
    }
    Dmod_Printf("DMA S4 %08x/%u->%u S6 %08x/%u->%u SAI SR %08x/%08x\n",
        (unsigned)DMA2_S4CR, (unsigned)tx_before, (unsigned)DMA2_S4NDTR,
        (unsigned)DMA2_S6CR, (unsigned)rx_before, (unsigned)DMA2_S6NDTR,
        (unsigned)SAI2_ASR, (unsigned)SAI2_BSR);
    CHECK(tx_progress);
    CHECK(rx_progress);
    const int16_t pcm[4] = {0, 0, 0, 0};
    size_t written = 0;
    rc = dmsai_port_write(1, pcm, sizeof(pcm), &written, 50);
    CHECK(rc == 0 && written == sizeof(pcm));
    int16_t *burst = Dmod_Malloc(300U * 2U * sizeof(int16_t));
    CHECK(burst != NULL);
    if (burst) {
        for (unsigned i = 0; i < 600U; ++i) burst[i] = (int16_t)i;
        written = 0;
        rc = dmsai_port_write(1, burst, 300U * 2U * sizeof(int16_t),
                              &written, 100);
        CHECK(rc == 0 && written == 300U * 2U * sizeof(int16_t));
        Dmod_Free(burst);
    }
    int16_t captured[2] = {0};
    size_t received = 0;
    rc = dmsai_port_read(1, captured, sizeof(captured), &received, 50);
    CHECK(rc == 0 && received == sizeof(captured));
    CHECK(dmsai_port_get_status(1, &status) == 0);
    CHECK(status.running);
    CHECK(status.transfer_errors == 0);
    Dmod_Printf("SAI status tx_underrun=%u rx_overrun=%u transfer_error=%u\n",
        (unsigned)status.tx_underruns, (unsigned)status.rx_overruns,
        (unsigned)status.transfer_errors);
    CHECK(dmsai_port_stop(1) == 0);
    CHECK(dmsai_port_deinit(1) == 0);
    CHECK((RCC_APB2ENR & SAI2EN) == 0);
    CHECK(RCC_CR == before_cr);
    CHECK(dmclk_port_get_domain_frequency(dmclk_domain_sai2) == before_sai);
}

/** @brief Run the STM32F746G-DISCO port checks as a DMOD executable. */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    pin_backup_t pins;
    setup_sai2_pins(&pins);
    CHECK(dmsai_port_get_instance_count() == 2);
    CHECK((RCC_APB2ENR & SAI2EN) == 0);
    exercise(48000);
    exercise(44100);
    restore_sai2_pins(&pins);
    Dmod_Printf("DMSAI PORT BOARD TEST: %s (%d failures)\n",
        failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
