#ifndef DMSAI_H
#define DMSAI_H
#include "dmod.h"
#include "dmsai_defs.h"
#include "dmsai_types.h"

/* Thread-context operations. A context must be serialized by its caller.
 * PCM buffers are borrowed until stop; callbacks run in IRQ context.
 * Buffers must be DMA-accessible and non-cacheable (e.g. dmheap "dma").
 * elements counts active-slot samples, not bytes/frames. 16-bit samples use
 * uint16_t; 24/32-bit use uint32_t with samples in the low bits. Each buffer
 * has two equal, whole-frame halves and remains owned by the caller.
 * No codec programming or pin routing is performed by this transport. */
dmod_dmsai_api(1.0, int, _create, ( const dmsai_config_t *config, dmsai_context_t *context ) );
dmod_dmsai_api(1.0, int, _destroy, ( dmsai_context_t context ) );
dmod_dmsai_api(1.0, int, _start, ( dmsai_context_t context, const void *tx, void *rx, size_t elements, dmsai_callback_t callback, void *user ) );
dmod_dmsai_api(1.0, int, _stop, ( dmsai_context_t context ) );
dmod_dmsai_api(1.0, int, _get_status, ( dmsai_context_t context, dmsai_status_t *status ) );
#endif
