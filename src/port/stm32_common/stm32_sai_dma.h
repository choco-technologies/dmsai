#ifndef DMSAI_STM32_SAI_DMA_H
#define DMSAI_STM32_SAI_DMA_H

#include <stddef.h>
#include <stdint.h>
#include "dmsai_types.h"
#include "stm32_sai.h"

typedef struct stm32_sai_dma stm32_sai_dma_t;

/**
 * @brief Reserve DMA streams and allocate uncached circular SAI buffers.
 * @param result Receives the created context on success, or NULL on failure.
 * @param route Family-specific DMA controller, stream and request mapping.
 * @param tx_dr Address of the transmitting SAI data register.
 * @param rx_dr Address of the receiving SAI data register.
 * @param config Validated port configuration retained by the caller.
 * @param status Mutable counters retained by the caller.
 * @return 0 on success or a negative errno value.
 */
int stm32_sai_dma_create(stm32_sai_dma_t **result,
    const stm32_sai_dma_route_t *route, uintptr_t tx_dr, uintptr_t rx_dr,
    const dmsai_config_t *config, dmsai_status_t *status);

/**
 * @brief Abort transfers, release their leases and free all DMA storage.
 * @param dma Context to destroy; NULL is allowed.
 */
void stm32_sai_dma_destroy(stm32_sai_dma_t *dma);

/**
 * @brief Start circular DMA for the configured SAI directions.
 * @param dma Initialized context.
 * @return 0 on success or a negative errno value.
 */
int stm32_sai_dma_start(stm32_sai_dma_t *dma);

/**
 * @brief Stop circular DMA before disabling the SAI blocks.
 * @param dma Context to stop; NULL is allowed.
 */
void stm32_sai_dma_stop(stm32_sai_dma_t *dma);

/**
 * @brief Copy complete PCM frames into the software TX queue.
 * @param dma Running context.
 * @param buffer Source PCM bytes.
 * @param size Number of bytes to copy.
 * @param written Receives accepted byte count.
 * @param timeout_ms Wait limit, or zero for no limit.
 * @return 0 if any frames were accepted, otherwise a negative errno value.
 */
int stm32_sai_dma_write(stm32_sai_dma_t *dma, const void *buffer,
    size_t size, size_t *written, uint32_t timeout_ms);

/**
 * @brief Wait for queued TX data and its DMA half to finish transmission.
 * @param dma Running context.
 * @param timeout_ms Wait limit, or zero for no limit.
 * @return 0 when drained or a negative errno on timeout, stop or DMA fault.
 */
int stm32_sai_dma_flush(stm32_sai_dma_t *dma, uint32_t timeout_ms);

/**
 * @brief Copy complete PCM frames out of the software RX queue.
 * @param dma Running context.
 * @param buffer Destination for PCM bytes.
 * @param size Number of bytes to copy.
 * @param received Receives copied byte count.
 * @param timeout_ms Wait limit, or zero for no limit.
 * @return 0 if any frames were copied, otherwise a negative errno value.
 */
int stm32_sai_dma_read(stm32_sai_dma_t *dma, void *buffer,
    size_t size, size_t *received, uint32_t timeout_ms);

#endif
