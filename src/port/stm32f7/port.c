#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"

/* Family-specific lifecycle and, later, interrupt routing only. */
/**
 * @brief Initialize the STM32F7 port module without touching hardware.
 * @param config DMOD loader configuration; unused by this interface stub.
 * @return 0 after the module is loaded.
 */
int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    Dmod_Printf("dmsai_port STM32F7 stub loaded\n");
    return 0;
}

/**
 * @brief Deinitialize the STM32F7 port module.
 * @return 0; no hardware resources are held by this stub.
 */
int dmod_deinit(void)
{
    return 0;
}
