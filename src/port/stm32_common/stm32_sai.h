#ifndef DMSAI_STM32_SAI_H
#define DMSAI_STM32_SAI_H

#include <stdint.h>
#include <stdbool.h>
#include "dmclk_port.h"

typedef struct
{
    uint8_t controller;
    uint8_t tx_stream;
    uint8_t rx_stream;
    uint16_t tx_request;
    uint16_t rx_request;
} stm32_sai_dma_route_t;

/* RCC and SAI addresses vary by family. Keep the register-based SAI engine
 * common; each family supplies only this small hardware descriptor. */
typedef struct
{
    uintptr_t rcc_base;
    uintptr_t sai_base[2];
    uint32_t sai_enable[2];
    dmclk_domain_t clock_domain[2];
    stm32_sai_dma_route_t dma[2];
    uint8_t count;
} stm32_sai_family_t;

/**
 * @brief Select the register layout and instances supplied by an STM32 family.
 * @param family Immutable descriptor kept alive for the module's lifetime.
 */
void stm32_sai_set_family(const stm32_sai_family_t *family);

/**
 * @brief Reject module unload while a SAI controller is configured.
 * @return 0 when the port is idle, or -EBUSY while a controller is in use.
 */
int stm32_sai_can_unload(void);

#endif
