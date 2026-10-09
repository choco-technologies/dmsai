#define DMOD_ENABLE_REGISTRATION ON
#include "dmsai_port.h"
#include "stm32_sai.h"
#include "stm32_sai_dma.h"
#include "dmod.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

/* The STM32F4/F7 SAI register block is shared. Family-specific addresses and
 * clock domains come from the descriptor in stm32f7/port.c. */
#define RCC_APB2ENR 0x44U
#define RCC_APB2RSTR 0x24U
#define SAI_BLOCK_A 0x04U
#define SAI_BLOCK_B 0x24U
#define SAI_CR1 0x00U
#define SAI_CR2 0x04U
#define SAI_FRCR 0x08U
#define SAI_SLOTR 0x0CU
#define SAI_IMR 0x10U
#define SAI_SR 0x14U
#define SAI_CLRFR 0x18U
#define SAI_DR 0x1CU
#define SAIEN (1U << 16)
#define SAI_DMAEN (1U << 17)
#define FIFO_FLUSH (1U << 3)
#define ERROR_FLAGS 0x75U
#define MCKDIV 2U
#define KERNEL_CLOCK_FACTOR (256U * 2U * MCKDIV)

typedef struct {
    dmsai_config_t config;
    dmsai_status_t status;
    uint8_t sample_bytes;
    uint8_t active_count;
    bool initialized;
    bool busy;
    bool tx_busy;
    bool rx_busy;
    stm32_sai_dma_t *dma;
} sai_state_t;

static const stm32_sai_family_t *family;
static sai_state_t states[2];

/** @brief Return an RCC register selected by the active family descriptor. */
static volatile uint32_t *rcc_reg(uint32_t offset)
{
    return (volatile uint32_t *)(family->rcc_base + offset);
}

/** @brief Return a register in one SAI controller's A or B block. */
static volatile uint32_t *block_reg(unsigned instance, bool block_b,
                                    uint32_t offset)
{
    return (volatile uint32_t *)(family->sai_base[instance] +
        (block_b ? SAI_BLOCK_B : SAI_BLOCK_A) + offset);
}

/** @brief Find the local state for a valid controller instance. */
static sai_state_t *get_state(dmsai_instance_t instance)
{
    return family && instance < family->count ? &states[instance] : NULL;
}

/** @brief Count the enabled PCM slots in a frame. */
static uint8_t count_slots(uint32_t slots)
{
    uint8_t count = 0;
    while (slots) {
        count += (uint8_t)(slots & 1U);
        slots >>= 1;
    }
    return count;
}

/** @brief Validate a portable configuration against STM32 SAI limits. */
static int validate(const dmsai_config_t *config)
{
    if (!config || !get_state(config->instance) || !config->sample_rate_hz ||
        config->sample_rate_hz > 96000 || config->tolerance_ppm > 1000000 ||
        (unsigned)config->clock_role > dmsai_clock_slave ||
        (!config->transmit && !config->receive))
        return -EINVAL;
    if (config->clock_role == dmsai_clock_slave) return -ENOTSUP;
    if (config->slot_count < 2 || config->slot_count > 16 ||
        (config->slot_count & 1U) ||
        (config->slot_bits != 16 && config->slot_bits != 32) ||
        config->frame_bits != config->slot_count * config->slot_bits ||
        !config->active_slots ||
        (config->active_slots & ~((1U << config->slot_count) - 1U)))
        return -EINVAL;
    if (config->frame_bits > 256) return -ENOTSUP;
    if (config->framing != dmsai_frame_i2s &&
        config->framing != dmsai_frame_tdm)
        return -EINVAL;
    if (config->framing == dmsai_frame_i2s && config->slot_count != 2)
        return -ENOTSUP;
    if ((config->pcm_format == dmsai_pcm_s16_le && config->slot_bits != 16) ||
        ((config->pcm_format == dmsai_pcm_s24_in32_le ||
          config->pcm_format == dmsai_pcm_s32_le) && config->slot_bits != 32))
        return -ENOTSUP;
    return (unsigned)config->pcm_format > dmsai_pcm_s32_le ? -EINVAL : 0;
}

/** @brief Encode a PCM sample width for the SAI data-size field. */
static uint32_t data_size(dmsai_pcm_format_t format)
{
    return (format == dmsai_pcm_s16_le ? 4U :
            format == dmsai_pcm_s24_in32_le ? 6U : 7U) << 5;
}

/** @brief Program framing, slot and transfer registers for one SAI block. */
static void configure_block(unsigned instance, bool block_b,
                            const dmsai_config_t *config)
{
    uint32_t cr1 = data_size(config->pcm_format) | (1U << 9);
    if (block_b)
        cr1 |= 3U | (1U << 10); /* Slave RX, synchronous with block A. */
    else
        cr1 |= (1U << 13) | (MCKDIV << 20);
    uint32_t frcr = (config->frame_bits - 1U) |
        (((config->frame_bits / 2U) - 1U) << 8) |
        (1U << 16) | (1U << 18);
    uint32_t slotr = ((config->slot_bits == 16 ? 1U : 2U) << 6) |
        ((config->slot_count - 1U) << 8) | (config->active_slots << 16);
    *block_reg(instance, block_b, SAI_CR1) = cr1;
    *block_reg(instance, block_b, SAI_CR2) =
        FIFO_FLUSH | (block_b ? 0U : 4U); /* Fill TX FIFO before first frame. */
    *block_reg(instance, block_b, SAI_FRCR) = frcr;
    *block_reg(instance, block_b, SAI_SLOTR) = slotr;
    *block_reg(instance, block_b, SAI_IMR) = 0;
    *block_reg(instance, block_b, SAI_CLRFR) = 0x7FU;
}

/** @brief Accumulate and clear the hardware error flags of active blocks. */
static void count_errors(unsigned instance, sai_state_t *state)
{
    for (unsigned block = 0; block < 2; ++block) {
        if (block && !state->config.receive) continue;
        uint32_t flags = *block_reg(instance, block != 0, SAI_SR) & ERROR_FLAGS;
        if (flags & 1U) {
            uint32_t *counter = block ? &state->status.rx_overruns :
                                        &state->status.tx_underruns;
            __atomic_fetch_add(counter, 1U, __ATOMIC_RELAXED);
        }
        if (flags & ~1U)
            __atomic_fetch_add(&state->status.transfer_errors, 1U,
                               __ATOMIC_RELAXED);
        if (flags) *block_reg(instance, block != 0, SAI_CLRFR) = flags;
    }
}

/** @copydoc stm32_sai_set_family */
void stm32_sai_set_family(const stm32_sai_family_t *selected)
{
    family = selected;
}

/** @copydoc stm32_sai_can_unload */
int stm32_sai_can_unload(void)
{
    for (unsigned i = 0; i < 2; ++i)
        if (states[i].initialized || states[i].busy) return -EBUSY;
    family = NULL;
    return 0;
}

/** @copydoc dmsai_port_get_instance_count */
dmod_dmsai_port_api_declaration(1.0, int, _get_instance_count, ( void ))
{
    return family ? family->count : -ENODEV;
}

/** @copydoc dmsai_port_init */
dmod_dmsai_port_api_declaration(1.0, int, _init,
    ( const dmsai_config_t *config ))
{
    int rc = validate(config);
    if (rc != 0) return rc;
    unsigned index = config->instance;
    if (family->dma[index].tx_stream == UINT8_MAX) return -ENOTSUP;
    sai_state_t *state = &states[index];
    if (state->initialized || state->busy ||
        (*rcc_reg(RCC_APB2ENR) & family->sai_enable[index]))
        return -EBUSY;
    state->busy = true;
    dmclk_frequency_t target = (dmclk_frequency_t)config->sample_rate_hz *
        KERNEL_CLOCK_FACTOR;
    dmclk_frequency_t tolerance =
        (target * config->tolerance_ppm + 999999U) / 1000000U;
    dmclk_frequency_t actual;
    rc = dmclk_port_acquire_domain(family->clock_domain[index], target,
                                    tolerance, &actual);
    if (rc == 0) {
        *rcc_reg(RCC_APB2ENR) |= family->sai_enable[index];
        *(volatile uint32_t *)family->sai_base[index] = 0; /* GCR */
        configure_block(index, false, config);
        if (config->receive) configure_block(index, true, config);
        memset(state, 0, sizeof(*state));
        state->busy = true;
        state->config = *config;
        state->sample_bytes = config->slot_bits / 8U;
        state->active_count = count_slots(config->active_slots);
        state->status.actual_sample_rate_hz =
            (uint32_t)((actual + KERNEL_CLOCK_FACTOR / 2U) /
                       KERNEL_CLOCK_FACTOR);
        rc = stm32_sai_dma_create(&state->dma, &family->dma[index],
            (uintptr_t)block_reg(index, false, SAI_DR),
            (uintptr_t)block_reg(index, true, SAI_DR), config,
            &state->status);
        if (rc == 0) state->initialized = true;
        else {
            *rcc_reg(RCC_APB2ENR) &= ~family->sai_enable[index];
            dmclk_port_release_domain(family->clock_domain[index]);
        }
    }
    state->busy = false;
    return rc;
}

/** @copydoc dmsai_port_stop */
dmod_dmsai_port_api_declaration(1.0, int, _stop,
    ( dmsai_instance_t instance ))
{
    sai_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    __atomic_store_n(&state->status.running, false, __ATOMIC_RELEASE);
    *block_reg(instance, false, SAI_CR1) &= ~SAI_DMAEN;
    if (state->config.receive)
        *block_reg(instance, true, SAI_CR1) &= ~SAI_DMAEN;
    stm32_sai_dma_stop(state->dma);
    if (state->config.receive)
        *block_reg(instance, true, SAI_CR1) &= ~SAIEN;
    *block_reg(instance, false, SAI_CR1) &= ~SAIEN;
    Dmod_Timestamp_t started = Dmod_GetUptime();
    bool timed_out = false;
    while ((*block_reg(instance, false, SAI_CR1) & SAIEN) ||
           (state->config.receive &&
            (*block_reg(instance, true, SAI_CR1) & SAIEN))) {
        if (Dmod_GetUptime() - started >= 5U) {
            timed_out = true;
            break;
        }
    }
    count_errors(instance, state);
    if (timed_out) {
        /* A synchronous slave can wait indefinitely for a frame boundary.
         * Reset only this controller, then restore its configuration. */
        *rcc_reg(RCC_APB2RSTR) |= family->sai_enable[instance];
        *rcc_reg(RCC_APB2RSTR) &= ~family->sai_enable[instance];
        if (*rcc_reg(RCC_APB2RSTR) & family->sai_enable[instance])
            return -ETIMEDOUT;
        *(volatile uint32_t *)family->sai_base[instance] = 0;
        configure_block(instance, false, &state->config);
        if (state->config.receive)
            configure_block(instance, true, &state->config);
        __atomic_fetch_add(&state->status.transfer_errors, 1U,
                           __ATOMIC_RELAXED);
        return 0;
    }
    *block_reg(instance, false, SAI_CR2) |= FIFO_FLUSH;
    if (state->config.receive)
        *block_reg(instance, true, SAI_CR2) |= FIFO_FLUSH;
    return 0;
}

/** @copydoc dmsai_port_deinit */
dmod_dmsai_port_api_declaration(1.0, int, _deinit,
    ( dmsai_instance_t instance ))
{
    sai_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    if (state->busy || state->tx_busy || state->rx_busy) return -EBUSY;
    int rc = dmsai_port_stop(instance);
    if (rc != 0) return rc;
    stm32_sai_dma_destroy(state->dma);
    state->dma = NULL;
    *rcc_reg(RCC_APB2ENR) &= ~family->sai_enable[instance];
    rc = dmclk_port_release_domain(family->clock_domain[instance]);
    if (rc == 0) memset(state, 0, sizeof(*state));
    return rc;
}

/** @copydoc dmsai_port_start */
dmod_dmsai_port_api_declaration(1.0, int, _start,
    ( dmsai_instance_t instance ))
{
    sai_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    if (__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return 0;
    *block_reg(instance, false, SAI_CLRFR) = 0x7FU;
    if (state->config.receive) {
        *block_reg(instance, true, SAI_CLRFR) = 0x7FU;
    }
    int rc = stm32_sai_dma_start(state->dma);
    if (rc != 0) return rc;
    if (state->config.receive)
        *block_reg(instance, true, SAI_CR1) |= SAIEN;
    __atomic_store_n(&state->status.running, true, __ATOMIC_RELEASE);
    *block_reg(instance, false, SAI_CR1) |= SAIEN;
    *block_reg(instance, false, SAI_CR1) |= SAI_DMAEN;
    if (state->config.receive)
        *block_reg(instance, true, SAI_CR1) |= SAI_DMAEN;
    return 0;
}

/** @copydoc dmsai_port_read */
dmod_dmsai_port_api_declaration(1.0, int, _read,
    ( dmsai_instance_t instance, void *buffer, size_t size, size_t *received,
      uint32_t timeout_ms ))
{
    sai_state_t *state = get_state(instance);
    if (!state || !state->initialized || !state->config.receive) return -ENODEV;
    size_t frame_bytes = state->active_count * state->sample_bytes;
    if (!received || (!buffer && size) || size % frame_bytes) return -EINVAL;
    *received = 0;
    if (!__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return -EPIPE;
    if (__atomic_exchange_n(&state->rx_busy, true, __ATOMIC_ACQ_REL))
        return -EBUSY;
    int rc = stm32_sai_dma_read(state->dma, buffer, size, received, timeout_ms);
    count_errors(instance, state);
    __atomic_store_n(&state->rx_busy, false, __ATOMIC_RELEASE);
    return rc;
}

/** @copydoc dmsai_port_write */
dmod_dmsai_port_api_declaration(1.0, int, _write,
    ( dmsai_instance_t instance, const void *buffer, size_t size,
      size_t *written, uint32_t timeout_ms ))
{
    sai_state_t *state = get_state(instance);
    if (!state || !state->initialized || !state->config.transmit) return -ENODEV;
    size_t frame_bytes = state->active_count * state->sample_bytes;
    if (!written || (!buffer && size) || size % frame_bytes) return -EINVAL;
    *written = 0;
    if (!__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return -EPIPE;
    if (__atomic_exchange_n(&state->tx_busy, true, __ATOMIC_ACQ_REL))
        return -EBUSY;
    int rc = stm32_sai_dma_write(state->dma, buffer, size, written, timeout_ms);
    count_errors(instance, state);
    __atomic_store_n(&state->tx_busy, false, __ATOMIC_RELEASE);
    return rc;
}

/** @copydoc dmsai_port_get_status */
dmod_dmsai_port_api_declaration(1.0, int, _get_status,
    ( dmsai_instance_t instance, dmsai_status_t *status ))
{
    sai_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    if (!status) return -EINVAL;
    count_errors(instance, state);
    *status = state->status;
    return 0;
}
