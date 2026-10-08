#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "../stm32_common/stm32_sai.h"

static const stm32_sai_family_t stm32f7_sai = {
    .rcc_base = 0x40023800U,
    .sai_base = {0x40015800U, 0x40015C00U},
    .sai_enable = {1U << 22, 1U << 23},
    .clock_domain = {dmclk_domain_sai1, dmclk_domain_sai2},
    .count = 2,
};

/* Family-specific lifecycle and, later, interrupt routing only. */
/**
 * @brief Install the STM32F7 SAI hardware descriptor.
 * @param config DMOD loader configuration; unused by the port.
 * @return 0 after the descriptor is installed.
 */
int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    stm32_sai_set_family(&stm32f7_sai);
    return 0;
}

/**
 * @brief Deinitialize the STM32F7 port module.
 * @return 0 when no controller is configured, otherwise -EBUSY.
 */
int dmod_deinit(void)
{
    return stm32_sai_can_unload();
}
