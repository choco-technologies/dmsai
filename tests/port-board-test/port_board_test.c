#include "dmsai_port.h"
#include "dmclk_port.h"
#include "dmod.h"
#include <errno.h>

#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844U)
#define RCC_CR      (*(volatile uint32_t *)0x40023800U)
#define SAI2_ACR1   (*(volatile uint32_t *)0x40015C04U)
#define SAI2_BCR1   (*(volatile uint32_t *)0x40015C24U)
#define SAI2_AFRCR  (*(volatile uint32_t *)0x40015C0CU)
#define SAI2_ASLOTR (*(volatile uint32_t *)0x40015C10U)
#define SAI2EN (1U << 23)

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
    const int16_t pcm[4] = {0, 0, 0, 0};
    size_t written = 0;
    rc = dmsai_port_write(1, pcm, sizeof(pcm), &written, 50);
    CHECK(rc == 0 && written == sizeof(pcm));
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
    CHECK(dmsai_port_get_instance_count() == 2);
    CHECK((RCC_APB2ENR & SAI2EN) == 0);
    exercise(48000);
    exercise(44100);
    Dmod_Printf("DMSAI PORT BOARD TEST: %s (%d failures)\n",
        failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
