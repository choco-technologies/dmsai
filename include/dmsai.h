#ifndef DMSAI_H
#define DMSAI_H

#include "dmsai_defs.h"
#include "dmsai_types.h"

/**
 * @brief Create a stream and reserve its peripheral resources.
 *
 * The port validates whether the requested format, rate, directions and
 * instance are supported. The output is written only on success. Calls for
 * the same stream must be serialized by the caller.
 *
 * @param config Configuration borrowed for the duration of this call.
 * @param context Receives an opaque stream handle on success.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_api(1.0, int, _create,
    ( const dmsai_config_t *config, dmsai_context_t *context ));

/**
 * @brief Stop a stream and release its peripheral resources.
 *
 * A successful call invalidates the handle. The future implementation will
 * stop an active transfer before releasing it.
 *
 * @param context Handle returned by dmsai_create().
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_api(1.0, int, _destroy, ( dmsai_context_t context ));

/**
 * @brief Start circular PCM transfers for enabled directions.
 *
 * Supply `tx` only when transmit is enabled and `rx` only when receive is
 * enabled. Both buffers contain `elements` samples from active slots in
 * frame order. The value is a count of samples per buffer, not bytes or
 * frames; it must be divisible by twice the number of active slots. Each
 * half then contains whole frames. Samples occupy 16-bit elements for up to
 * 16 significant bits and 32-bit elements otherwise, with the significant
 * bits in the low part. Buffers must be aligned to their element width,
 * DMA-accessible on the target, distinct, and remain owned by the caller
 * until dmsai_stop() succeeds. Port-specific memory restrictions apply.
 *
 * @param context Configured stream.
 * @param tx Transmit buffer, or NULL when transmit is disabled.
 * @param rx Receive buffer, or NULL when receive is disabled.
 * @param elements Number of active-slot samples in each non-NULL buffer.
 * @param callback Optional transfer callback executed in interrupt context.
 * @param user Opaque pointer passed unchanged to callback.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_api(1.0, int, _start,
    ( dmsai_context_t context, const void *tx, void *rx, size_t elements,
      dmsai_callback_t callback, void *user ));

/**
 * @brief Stop circular transfers and detach the callback.
 *
 * After success the caller may reuse or release the PCM buffers.
 *
 * @param context Configured stream.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_api(1.0, int, _stop, ( dmsai_context_t context ));

/**
 * @brief Read the current stream state and cumulative event counters.
 *
 * @param context Configured stream.
 * @param status Receives a coherent snapshot on success; otherwise unchanged.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_api(1.0, int, _get_status,
    ( dmsai_context_t context, dmsai_status_t *status ));

#endif /* DMSAI_H */
