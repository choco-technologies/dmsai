#ifndef DMSAI_PORT_H
#define DMSAI_PORT_H

#include "dmsai_port_defs.h"
#include "dmsai_types.h"

/** @brief Opaque handle owned by the selected architecture port. */
typedef struct dmsai_port_context *dmsai_port_context_t;

/**
 * @brief Port callback for DMA and peripheral events.
 *
 * Runs in interrupt context; `user` is the pointer supplied to
 * dmsai_port_start().
 *
 * @param direction Direction that raised the event.
 * @param event Bitwise combination of dmsai_event_t flags.
 * @param user Caller-owned callback argument.
 */
typedef void (*dmsai_port_callback_t)(dmsai_direction_t direction,
                                       dmsai_event_t event, void *user);

/**
 * @brief Reserve and configure an architecture-specific SAI instance.
 *
 * The implementation validates the target's capabilities and writes both
 * outputs only on success. No data transfer begins until start.
 *
 * @param config Architecture-independent stream configuration.
 * @param context Receives the port handle on success.
 * @param actual_sample_rate_hz Receives the achievable frame rate in Hz.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_port_api(1.0, int, _create,
    ( const dmsai_config_t *config, dmsai_port_context_t *context,
      uint32_t *actual_sample_rate_hz ));

/**
 * @brief Stop and release a previously created port context.
 *
 * @param context Handle returned by dmsai_port_create().
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_port_api(1.0, int, _destroy, ( dmsai_port_context_t context ));

/**
 * @brief Start circular transfers through the selected hardware instance.
 *
 * The buffer contract is the same as dmsai_start(). DMA and peripheral
 * events are forwarded to callback from interrupt context.
 *
 * @param context Configured port context.
 * @param tx Transmit samples, or NULL if transmit is disabled.
 * @param rx Receive samples, or NULL if receive is disabled.
 * @param elements Samples in each non-NULL buffer.
 * @param callback Optional event callback.
 * @param user Callback argument.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_port_api(1.0, int, _start,
    ( dmsai_port_context_t context, const void *tx, void *rx, size_t elements,
      dmsai_port_callback_t callback, void *user ));

/**
 * @brief Stop all transfers and make buffers safe to release.
 *
 * @param context Configured port context.
 * @return 0 on success or a negative errno value. The stub returns -ENOSYS.
 */
dmod_dmsai_port_api(1.0, int, _stop, ( dmsai_port_context_t context ));

#endif /* DMSAI_PORT_H */
