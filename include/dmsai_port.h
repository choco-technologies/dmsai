#ifndef DMSAI_PORT_H
#define DMSAI_PORT_H

#include <stddef.h>
#include <stdint.h>
#include "dmsai_port_defs.h"
#include "dmsai_types.h"

/**
 * @brief Report how many SAI controllers the selected target exposes.
 *
 * @return Non-negative instance count, or -ENODEV before port initialization.
 */
dmod_dmsai_port_api(1.0, int, _get_instance_count, ( void ));

/**
 * @brief Reserve and configure one controller without starting data flow.
 *
 * The core owns the dmdevfs and dmdrvi context; the port owns only the
 * hardware resources. Board pin configuration and codec control are separate.
 *
 * @param config Portable configuration, valid for this call.
 * @return 0 on success; -EINVAL for malformed configuration, -ENOTSUP for
 * an unsupported format, -EBUSY when the controller is in use, or an error
 * from the clock provider.
 */
dmod_dmsai_port_api(1.0, int, _init, ( const dmsai_config_t *config ));

/**
 * @brief Stop and release one configured controller.
 *
 * @param instance Zero-based controller index.
 * @return 0 on success, -ENODEV if not initialized, -EBUSY while I/O is
 * active, or a clock/hardware error.
 */
dmod_dmsai_port_api(1.0, int, _deinit, ( dmsai_instance_t instance ));

/**
 * @brief Enable the configured TX and/or RX stream.
 *
 * @param instance Zero-based controller index.
 * @return 0 on success or -ENODEV if not initialized.
 */
dmod_dmsai_port_api(1.0, int, _start, ( dmsai_instance_t instance ));

/**
 * @brief Disable the stream and wake blocked readers and writers.
 *
 * The port may reset and reconfigure its controller if a synchronous block
 * cannot finish the current frame. The transfer error counter then increases.
 *
 * @param instance Zero-based controller index.
 * @return 0 on success, -ENODEV if not initialized, or -ETIMEDOUT if the
 * hardware does not acknowledge disable.
 */
dmod_dmsai_port_api(1.0, int, _stop, ( dmsai_instance_t instance ));

/**
 * @brief Copy complete PCM frames from RX into a caller buffer.
 *
 * `size` and `received` count bytes. A successful short transfer is allowed;
 * only complete frames may be returned. The implementation may block up to
 * timeout_ms while waiting for samples. A zero timeout means wait forever.
 *
 * @param instance Zero-based controller index.
 * @param buffer Destination buffer.
 * @param size Available bytes, a multiple of the PCM frame size.
 * @param received Receives copied byte count on success.
 * @param timeout_ms Maximum wait in milliseconds, or zero for no limit.
 * @return 0 on success, including a short whole-frame transfer after a
 * timeout; otherwise a negative errno value.
 */
dmod_dmsai_port_api(1.0, int, _read,
    ( dmsai_instance_t instance, void *buffer, size_t size, size_t *received,
      uint32_t timeout_ms ));

/**
 * @brief Copy complete PCM frames from a caller buffer into TX.
 *
 * `size` and `written` count bytes. A successful short transfer is allowed;
 * only complete frames may be accepted. The implementation may block up to
 * timeout_ms while waiting for free space. A zero timeout means wait forever.
 *
 * @param instance Zero-based controller index.
 * @param buffer Source buffer.
 * @param size Source bytes, a multiple of the PCM frame size.
 * @param written Receives accepted byte count on success.
 * @param timeout_ms Maximum wait in milliseconds, or zero for no limit.
 * @return 0 on success, including a short whole-frame transfer after a
 * timeout; otherwise a negative errno value.
 */
dmod_dmsai_port_api(1.0, int, _write,
    ( dmsai_instance_t instance, const void *buffer, size_t size,
      size_t *written, uint32_t timeout_ms ));

/**
 * @brief Wait until all previously accepted TX frames reach the output.
 *
 * A zero timeout waits indefinitely. STOP interrupts the wait. Callers must
 * serialize writes and flushes on the same stream.
 *
 * @param instance Zero-based controller index.
 * @param timeout_ms Maximum wait in milliseconds, or zero for no limit.
 * @return 0 when drained, -ETIMEDOUT on timeout, -ECANCELED after STOP,
 * or a negative errno value for an unconfigured or failed stream.
 */
dmod_dmsai_port_api(1.0, int, _flush,
    ( dmsai_instance_t instance, uint32_t timeout_ms ));

/**
 * @brief Take a coherent snapshot of transfer state and counters.
 *
 * @param instance Zero-based controller index.
 * @param status Receives status on success and remains unchanged on failure.
 * @return 0 on success, -ENODEV if not initialized, or -EINVAL for a null
 * output pointer.
 */
dmod_dmsai_port_api(1.0, int, _get_status,
    ( dmsai_instance_t instance, dmsai_status_t *status ));

#endif /* DMSAI_PORT_H */
