#include "dmsai_port.h"
#include "dmclk_port.h"
#include "dmod.h"
#include <errno.h>

#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844U)
#define RCC_CR      (*(volatile uint32_t *)0x40023800U)
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830U)
#define SAI_BASE 0x40015800U
#define DMA2_BASE 0x40026400U
#define GPIOE_BASE 0x40021000U
#define GPIOG_BASE 0x40021800U
#define GPIOI_BASE 0x40022000U

typedef struct {
    uint32_t ahb1enr;
    uint32_t g_moder, g_otyper, g_ospeedr, g_pupdr, g_afrh;
    uint32_t i_moder, i_otyper, i_ospeedr, i_pupdr, i_afrl;
} pin_backup_t;

typedef struct {
    uint32_t ahb1enr;
    uint32_t moder, otyper, ospeedr, pupdr, afrl;
} sai1_pin_backup_t;

/** @brief Access one register of an SAI instance and audio block. */
static volatile uint32_t *sai_reg(unsigned instance, bool block_b,
                                  uint32_t offset)
{
    return (volatile uint32_t *)(SAI_BASE + instance * 0x400U +
        (block_b ? 0x20U : 0U) + offset);
}

/** @brief Access one DMA2 stream register. */
static volatile uint32_t *dma_reg(unsigned stream, uint32_t offset)
{
    return (volatile uint32_t *)(DMA2_BASE + 0x10U + stream * 0x18U + offset);
}

/** @brief Access a GPIO register on the board's SAI2 pin banks. */
static volatile uint32_t *gpio_reg(uintptr_t base, uint32_t offset)
{
    return (volatile uint32_t *)(base + offset);
}

/** @brief Route SAI1 A clocks/TX and block B RX through port E AF6. */
static void setup_sai1_pins(sai1_pin_backup_t *backup)
{
    backup->ahb1enr = RCC_AHB1ENR;
    RCC_AHB1ENR |= 1U << 4;
    backup->moder = *gpio_reg(GPIOE_BASE, 0);
    backup->otyper = *gpio_reg(GPIOE_BASE, 4);
    backup->ospeedr = *gpio_reg(GPIOE_BASE, 8);
    backup->pupdr = *gpio_reg(GPIOE_BASE, 12);
    backup->afrl = *gpio_reg(GPIOE_BASE, 32);
    for (unsigned pin = 2; pin <= 6; ++pin) {
        *gpio_reg(GPIOE_BASE, 0) =
            (*gpio_reg(GPIOE_BASE, 0) & ~(3U << (pin * 2))) |
            (2U << (pin * 2));
        *gpio_reg(GPIOE_BASE, 4) &= ~(1U << pin);
        *gpio_reg(GPIOE_BASE, 8) |= 3U << (pin * 2);
        *gpio_reg(GPIOE_BASE, 12) &= ~(3U << (pin * 2));
        *gpio_reg(GPIOE_BASE, 32) =
            (*gpio_reg(GPIOE_BASE, 32) & ~(15U << (pin * 4))) |
            (6U << (pin * 4));
    }
}

/** @brief Restore port E after the direct SAI1 test. */
static void restore_sai1_pins(const sai1_pin_backup_t *backup)
{
    *gpio_reg(GPIOE_BASE, 0) = backup->moder;
    *gpio_reg(GPIOE_BASE, 4) = backup->otyper;
    *gpio_reg(GPIOE_BASE, 8) = backup->ospeedr;
    *gpio_reg(GPIOE_BASE, 12) = backup->pupdr;
    *gpio_reg(GPIOE_BASE, 32) = backup->afrl;
    RCC_AHB1ENR = backup->ahb1enr;
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

/** @brief Configure, transfer on and release one SAI at one sample rate. */
static void exercise(unsigned instance, uint32_t rate)
{
    dmsai_config_t config = {
        .instance = instance, .clock_role = dmsai_clock_master,
        .framing = dmsai_frame_tdm, .pcm_format = dmsai_pcm_s16_le,
        .sample_rate_hz = rate, .tolerance_ppm = 500,
        .slot_bits = 16, .slot_count = 4, .frame_bits = 64,
        .active_slots = 5, .transmit = true, .receive = true,
    };
    unsigned tx_stream = instance ? 4U : 1U;
    unsigned rx_stream = instance ? 6U : 5U;
    dmclk_domain_t domain = instance ? dmclk_domain_sai2 : dmclk_domain_sai1;
    uint32_t gate = 1U << (22U + instance);
    volatile uint32_t *acr1 = sai_reg(instance, false, 4);
    volatile uint32_t *bcr1 = sai_reg(instance, true, 4);
    uint32_t before_cr = RCC_CR;
    dmclk_frequency_t before_sai = dmclk_port_get_domain_frequency(domain);
    int rc = dmsai_port_init(&config);
    Dmod_Printf("DMSAI SAI%u init rate=%u rc=%d\n",
        instance + 1U, (unsigned)rate, rc);
    CHECK(rc == 0);
    if (rc != 0) return;
    CHECK(dmsai_port_init(&config) == -EBUSY);
    CHECK((RCC_APB2ENR & gate) != 0);
    CHECK((*acr1 & (15U << 20)) == (2U << 20));
    CHECK((*bcr1 & (3U << 10)) == (1U << 10));
    CHECK(*sai_reg(instance, false, 0x0CU) == 0x00051F3FU);
    CHECK(*sai_reg(instance, false, 0x10U) == 0x00050340U);
    dmsai_status_t status;
    CHECK(dmsai_port_get_status(instance, &status) == 0);
    uint32_t error = status.actual_sample_rate_hz > rate ?
        status.actual_sample_rate_hz - rate : rate - status.actual_sample_rate_hz;
    CHECK(error <= rate / 2000U);
    CHECK(dmsai_port_start(instance) == 0);
    CHECK((*acr1 & (1U << 16)) != 0);
    CHECK((*bcr1 & (1U << 16)) != 0);
    CHECK((*acr1 & (1U << 17)) != 0);
    CHECK((*bcr1 & (1U << 17)) != 0);
    CHECK((*dma_reg(tx_stream, 0) & 0x101U) == 0x101U);
    CHECK((*dma_reg(rx_stream, 0) & 0x101U) == 0x101U);
    CHECK(((*dma_reg(tx_stream, 0) >> 25) & 7U) == (instance ? 3U : 0U));
    CHECK(((*dma_reg(rx_stream, 0) >> 25) & 7U) == (instance ? 3U : 0U));
    uint32_t tx_before = *dma_reg(tx_stream, 4);
    uint32_t rx_before = *dma_reg(rx_stream, 4);
    bool tx_progress = false;
    bool rx_progress = false;
    for (unsigned i = 0; i < 10; ++i) {
        Dmod_ThreadSleep(1);
        if (*dma_reg(tx_stream, 4) != tx_before) tx_progress = true;
        if (*dma_reg(rx_stream, 4) != rx_before) rx_progress = true;
    }
    Dmod_Printf("DMA S%u %08x/%u->%u S%u %08x/%u->%u SAI SR %08x/%08x\n",
        tx_stream, (unsigned)*dma_reg(tx_stream, 0), (unsigned)tx_before,
        (unsigned)*dma_reg(tx_stream, 4), rx_stream,
        (unsigned)*dma_reg(rx_stream, 0), (unsigned)rx_before,
        (unsigned)*dma_reg(rx_stream, 4),
        (unsigned)*sai_reg(instance, false, 0x18U),
        (unsigned)*sai_reg(instance, true, 0x18U));
    CHECK(tx_progress);
    CHECK(rx_progress);
    const int16_t pcm[4] = {0, 0, 0, 0};
    size_t written = 0;
    rc = dmsai_port_write(instance, pcm, sizeof(pcm), &written, 50);
    CHECK(rc == 0 && written == sizeof(pcm));
    int16_t *burst = Dmod_Malloc(300U * 2U * sizeof(int16_t));
    CHECK(burst != NULL);
    if (burst) {
        for (unsigned i = 0; i < 600U; ++i) burst[i] = (int16_t)i;
        written = 0;
        rc = dmsai_port_write(instance, burst, 300U * 2U * sizeof(int16_t),
                              &written, 100);
        CHECK(rc == 0 && written == 300U * 2U * sizeof(int16_t));
        Dmod_Free(burst);
    }
    int16_t captured[2] = {0};
    size_t received = 0;
    rc = dmsai_port_read(instance, captured, sizeof(captured), &received, 50);
    CHECK(rc == 0 && received == sizeof(captured));
    CHECK(dmsai_port_get_status(instance, &status) == 0);
    CHECK(status.running);
    CHECK(status.transfer_errors == 0);
    Dmod_Printf("SAI status tx_underrun=%u rx_overrun=%u transfer_error=%u\n",
        (unsigned)status.tx_underruns, (unsigned)status.rx_overruns,
        (unsigned)status.transfer_errors);
    CHECK(dmsai_port_stop(instance) == 0);
    CHECK(dmsai_port_deinit(instance) == 0);
    CHECK((RCC_APB2ENR & gate) == 0);
    CHECK(RCC_CR == before_cr);
    CHECK(dmclk_port_get_domain_frequency(domain) == before_sai);
}

/** @brief Run the STM32F746G-DISCO port checks as a DMOD executable. */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    sai1_pin_backup_t sai1_pins;
    setup_sai1_pins(&sai1_pins);
    CHECK(dmsai_port_get_instance_count() == 2);
    CHECK((RCC_APB2ENR & ((1U << 22) | (1U << 23))) == 0);
    exercise(0, 48000);
    exercise(0, 44100);
    restore_sai1_pins(&sai1_pins);
    pin_backup_t sai2_pins;
    setup_sai2_pins(&sai2_pins);
    exercise(1, 48000);
    exercise(1, 44100);
    restore_sai2_pins(&sai2_pins);
    Dmod_Printf("DMSAI PORT BOARD TEST: %s (%d failures)\n",
        failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
