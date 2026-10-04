#ifndef DMSAI_VALIDATE_H
#define DMSAI_VALIDATE_H
#include "dmsai_types.h"
#include <errno.h>
static inline unsigned dmsai_slot_channels(uint16_t mask)
{
    unsigned channels = 0;
    while (mask) { channels += mask & 1; mask >>= 1; }
    return channels;
}
static inline int dmsai_validate_config(const dmsai_config_t *c)
{
    if (!c || c->instance != 2 || c->sample_rate < 8000 || c->sample_rate > 96000 ||
        c->tolerance_ppm > 10000 || (c->sample_bits != 16 && c->sample_bits != 24 && c->sample_bits != 32) ||
        (c->slot_bits != 16 && c->slot_bits != 32) || c->sample_bits > c->slot_bits ||
        c->slot_count < 2 || c->slot_count > 16 || (c->slot_count & 1) ||
        c->slot_count * c->slot_bits > 256 ||
        ((c->slot_count * c->slot_bits) & (c->slot_count * c->slot_bits - 1)) || !c->active_slots ||
        ((uint32_t)c->active_slots >> c->slot_count) || (dmsai_slot_channels(c->active_slots) & 1))
        return -EINVAL;
    return 0;
}
static inline int dmsai_validate_buffer(const dmsai_config_t *c, const void *tx, void *rx, size_t n)
{
    unsigned width = c->sample_bits == 16 ? 2 : 4;
    unsigned channels = dmsai_slot_channels(c->active_slots);
    if (!tx || (c->receive != (rx != NULL)) || n == 0 || n > 65535 ||
        (n % (2 * channels)) || ((uintptr_t)tx % width) || ((uintptr_t)rx % width))
        return -EINVAL;
    if (rx)
    {
        uintptr_t a = (uintptr_t)tx, b = (uintptr_t)rx;
        size_t bytes = n * width;
        if (a > UINTPTR_MAX - bytes || b > UINTPTR_MAX - bytes ||
            (a < b + bytes && b < a + bytes)) return -EINVAL;
    }
    return 0;
}
#endif
