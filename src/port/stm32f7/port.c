#define DMOD_ENABLE_REGISTRATION ON
#include "dmsai_port.h"
#include "dmclk_sai.h"
#include "dmdma_lease.h"
#include "../../dmsai_validate.h"
#include <errno.h>
#include <string.h>

#define MAGIC 0x53414950U
#define RCC(offset) (*(volatile uint32_t *)(0x40023800UL + (offset)))
#define SAI_GATE (1UL << 23)
#define SAI_ENABLE (1UL << 16)
#define SAI_DMA (1UL << 17)
#define SAI_ERRORS ((1UL << 0) | (1UL << 2) | (1UL << 5) | (1UL << 6))
#define IRQ 91U

typedef struct
{
    volatile uint32_t cr1, cr2, frcr, slotr, imr, sr, clrfr, dr;
} sai_block_t;
#define BLOCK(direction) ((sai_block_t *)(0x40015C04UL + (direction) * 0x20UL))

struct dmsai_port_context
{
    uint32_t magic;
    dmsai_config_t config;
    dmdma_lease_t dma[2];
    dmsai_port_callback_t callback;
    void *user;
    bool running;
};
static dmsai_port_context_t owner;
static bool claimed;
static bool valid(dmsai_port_context_t c) { return c && c->magic == MAGIC; }

static void nvic(bool enable)
{
    volatile uint32_t *clear = (uint32_t *)0xE000E180UL;
    volatile uint32_t *pending = (uint32_t *)0xE000E280UL;
    clear[IRQ / 32] = 1UL << (IRQ % 32);
    pending[IRQ / 32] = 1UL << (IRQ % 32);
    if (enable)
    {
        ((volatile uint8_t *)0xE000E400UL)[IRQ] = 14U << 4;
        ((volatile uint32_t *)0xE000E100UL)[IRQ / 32] = 1UL << (IRQ % 32);
    }
}

static bool claim(void)
{
    Dmod_EnterCritical();
    bool available = !claimed && !(RCC(0x44) & SAI_GATE);
    if (available) claimed = true;
    Dmod_ExitCritical();
    return available;
}
static void unclaim(void)
{
    Dmod_EnterCritical(); owner = NULL; claimed = false; Dmod_ExitCritical();
}

static void configure_block(dmsai_port_context_t c, unsigned direction)
{
    sai_block_t *b = BLOCK(direction);
    unsigned ds = c->config.sample_bits == 16 ? 4 : c->config.sample_bits == 24 ? 6 : 7;
    unsigned frame = c->config.slot_count * c->config.slot_bits;
    b->cr1 = ds << 5;
    if (direction == dmsai_tx) b->cr1 |= 1UL << 13; /* master TX, output drive */
    else b->cr1 |= 3U | (1UL << 9) | (1UL << 10); /* slave RX, rising, internal sync */
    b->cr2 = 1U | (1UL << 3); /* FIFO quarter threshold, flush */
    b->frcr = (frame - 1) | ((frame / 2 - 1) << 8) | (1UL << 16) | (1UL << 18);
    unsigned slot_size = c->config.slot_bits == 16 ? 1 : 2;
    b->slotr = (slot_size << 6) | ((c->config.slot_count - 1) << 8) |
                ((uint32_t)c->config.active_slots << 16);
    b->imr = 0; b->clrfr = 0x7FU;
}

static void dma_event(dmdma_lease_t lease, dmdma_event_t event, void *user)
{
    dmsai_port_context_t c = user;
    if (!valid(c) || !c->callback) return;
    dmsai_event_t converted = 0;
    if (event & dmdma_event_half_complete) converted |= dmsai_event_half;
    if (event & dmdma_event_complete) converted |= dmsai_event_complete;
    if (event & (dmdma_event_error | dmdma_event_timeout)) converted |= dmsai_event_dma_error;
    if (converted) c->callback(lease == c->dma[0] ? dmsai_tx : dmsai_rx, converted, c->user);
}

static void release_dma(dmsai_port_context_t c)
{
    for (unsigned i = 0; i < 2; i++)
        if (c->dma[i]) { dmdma_lease_release(c->dma[i]); c->dma[i] = NULL; }
}

static int create_clock(dmsai_port_context_t c, uint32_t *frequency)
{
    dmclk_frequency_t actual;
    uint32_t target = c->config.sample_rate * 256;
    uint32_t tolerance = ((uint64_t)target * c->config.tolerance_ppm) / 1000000;
    int result = dmclk_sai_acquire(dmclk_domain_sai2, target, tolerance, &actual);
    if (result) return result;
    RCC(0x44) |= SAI_GATE; (void)RCC(0x44);
    RCC(0x24) |= SAI_GATE; RCC(0x24) &= ~SAI_GATE;
    *(volatile uint32_t *)0x40015C00UL = 0; /* internal A->B sync, no external routing */
    configure_block(c, dmsai_tx);
    if (c->config.receive) configure_block(c, dmsai_rx);
    *frequency = actual;
    return 0;
}

int dmod_init(const Dmod_Config_t *config) { (void)config; return 0; }
int dmod_deinit(void) { return claimed ? -EBUSY : 0; }

dmod_dmsai_port_api_declaration(1.0, int, _create, ( const dmsai_config_t *config, dmsai_port_context_t *context, uint32_t *frequency ) )
{
    if (!context || !frequency) return -EINVAL;
    int result = dmsai_validate_config(config);
    if (result) return result;
    if (!claim()) return -EBUSY;
    dmsai_port_context_t c = Dmod_Malloc(sizeof(*c));
    if (!c) { unclaim(); return -ENOMEM; }
    memset(c, 0, sizeof(*c)); c->magic = MAGIC; c->config = *config;
    c->dma[0] = dmdma_lease_acquire(1, 4);
    if (config->receive) c->dma[1] = dmdma_lease_acquire(1, 7);
    result = (!c->dma[0] || (config->receive && !c->dma[1])) ? -EBUSY : create_clock(c, frequency);
    if (result)
    {
        release_dma(c); c->magic = 0; Dmod_Free(c); unclaim(); return result;
    }
    owner = c; *context = c;
    return 0;
}

/* Initial STM32F746 port deliberately accepts the non-cacheable DTCM heap.
 * Cached SRAM/SDRAM zero-copy needs an explicit cache maintenance contract. */
static bool dma_buffer(const void *buffer, size_t bytes)
{
    uintptr_t address = (uintptr_t)buffer;
    return address >= 0x20000000UL && address < 0x20010000UL && bytes <= 0x20010000UL - address;
}

static int start_dma(dmsai_port_context_t c, unsigned direction, void *buffer, size_t n)
{
    dmdma_data_width_t width = c->config.sample_bits == 16 ? dmdma_data_width_halfword : dmdma_data_width_word;
    dmdma_transfer_config_t transfer = {
        .direction = direction == dmsai_tx ? dmdma_direction_memory_to_peripheral : dmdma_direction_peripheral_to_memory,
        .request = direction == dmsai_tx ? 3 : 0,
        .source_address = direction == dmsai_tx ? buffer : (const void *)&BLOCK(direction)->dr,
        .destination_address = direction == dmsai_tx ? (void *)&BLOCK(direction)->dr : buffer,
        .source_width = width, .destination_width = width,
        .source_increment = direction == dmsai_tx, .destination_increment = direction == dmsai_rx,
        .circular = true, .priority = dmdma_priority_high, .element_count = n, .timeout_ms = 0
    };
    int result = dmdma_lease_set_callback(c->dma[direction], dma_event, c);
    if (result) return result;
    return dmdma_lease_start(c->dma[direction], &transfer);
}

dmod_dmsai_port_api_declaration(1.0, int, _start, ( dmsai_port_context_t context, const void *tx, void *rx, size_t elements, dmsai_port_callback_t callback, void *user ) )
{
    if (!valid(context)) return -EINVAL;
    if (context->running) return -EBUSY;
    int result = dmsai_validate_buffer(&context->config, tx, rx, elements);
    if (result) return result;
    size_t bytes = elements * (context->config.sample_bits == 16 ? 2 : 4);
    if (!dma_buffer(tx, bytes) || (rx && !dma_buffer(rx, bytes))) return -ENOTSUP;
    context->callback = callback; context->user = user;
    configure_block(context, dmsai_tx);
    if (rx) configure_block(context, dmsai_rx);
    result = start_dma(context, dmsai_tx, (void *)tx, elements);
    if (!result && rx) result = start_dma(context, dmsai_rx, rx, elements);
    if (result)
    {
        context->callback = NULL;
        for (unsigned i = 0; i < 2; i++) if (context->dma[i]) dmdma_lease_abort(context->dma[i]);
        return result;
    }
    __asm volatile ("dsb" ::: "memory");
    BLOCK(dmsai_tx)->imr = SAI_ERRORS;
    BLOCK(dmsai_tx)->cr1 |= SAI_DMA;
    if (rx)
    {
        BLOCK(dmsai_rx)->imr = SAI_ERRORS;
        BLOCK(dmsai_rx)->cr1 |= SAI_DMA | SAI_ENABLE;
    }
    context->running = true; nvic(true);
    BLOCK(dmsai_tx)->cr1 |= SAI_ENABLE;
    return 0;
}

static int disable_block(unsigned direction)
{
    sai_block_t *b = BLOCK(direction);
    b->imr = 0; b->cr1 &= ~SAI_ENABLE;
    for (unsigned i = 0; i < 1000000; i++)
        if (!(b->cr1 & SAI_ENABLE)) { b->cr1 &= ~SAI_DMA; return 0; }
    return -ETIMEDOUT;
}

dmod_dmsai_port_api_declaration(1.0, int, _stop, ( dmsai_port_context_t context ) )
{
    if (!valid(context)) return -EINVAL;
    if (!context->running) return 0;
    nvic(false);
    int result = context->config.receive ? disable_block(dmsai_rx) : 0;
    int tx_result = disable_block(dmsai_tx);
    if (!result) result = tx_result;
    if (result) return result; /* retain ownership until hardware is stopped */
    context->callback = NULL;
    for (unsigned i = 0; i < 2; i++)
    {
        if (!context->dma[i]) continue;
        dmdma_lease_abort(context->dma[i]);
        dmdma_lease_set_callback(context->dma[i], NULL, NULL);
    }
    context->running = false; context->user = NULL;
    return 0;
}

dmod_dmsai_port_api_declaration(1.0, int, _destroy, ( dmsai_port_context_t context ) )
{
    if (!valid(context)) return -EINVAL;
    int result = dmsai_port_stop(context);
    if (result) return result;
    nvic(false); RCC(0x44) &= ~SAI_GATE;
    result = dmclk_sai_release(dmclk_domain_sai2);
    if (result) { RCC(0x44) |= SAI_GATE; return result; }
    release_dma(context); unclaim(); context->magic = 0; Dmod_Free(context);
    return 0;
}

DMOD_IRQ_HANDLER(91)
{
    dmsai_port_context_t c = owner;
    if (!valid(c)) return;
    for (unsigned direction = 0; direction <= (unsigned)c->config.receive; direction++)
    {
        sai_block_t *b = BLOCK(direction);
        uint32_t errors = b->sr & b->imr & SAI_ERRORS;
        if (!errors) continue;
        b->clrfr = errors; b->imr &= ~errors; /* report once until next start */
        if (c->callback) c->callback(direction, dmsai_event_fifo_error, c->user);
    }
}
