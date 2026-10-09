#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmsai_private.h"
#include "dmsai_port.h"
#include "dmsai_ioctl.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

static uint32_t active_contexts;

/**
 * @brief Initialize the architecture-independent driver module.
 * @param config Loader configuration, unused.
 * @return 0 on success.
 */
int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    active_contexts = 0;
    return 0;
}

/**
 * @brief Deinitialize the driver when no dmdevfs nodes still own it.
 * @return 0 on success or -EBUSY while a node exists.
 */
int dmod_deinit(void)
{
    return __atomic_load_n(&active_contexts, __ATOMIC_ACQUIRE) ? -EBUSY : 0;
}

/** @brief Decode the access bits passed by dmdevfs or direct DIF callers. */
static int decode_access(int flags, bool *readable, bool *writable)
{
    /* dmdevfs forwards DMFSI_O_RDWR=3; dmdrvi.h also defines O_RDWR=4. */
    int access = flags & 7;
    if (access == DMDRVI_O_RDWR) access = 3;
    if (access < 1 || access > 3) return -EINVAL;
    *readable = (access & 1) != 0;
    *writable = (access & 2) != 0;
    return 0;
}

/**
 * @brief Parse the selected INI section and reserve one SAI controller.
 * @param config Section-restricted INI context supplied by dmdevfs.
 * @param dev_num Receives `/dev/dmsaiN` numbering on success.
 * @return Opaque driver context, or NULL if configuration or reservation fails.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, dmdrvi_context_t, _create,
    ( dmini_context_t config, dmdrvi_dev_num_t *dev_num ))
{
    if (!config || !dev_num) return NULL;
    dmdrvi_context_t context = Dmod_Malloc(sizeof(*context));
    if (!context) return NULL;
    memset(context, 0, sizeof(*context));
    int rc = dmsai_parse_config(config, &context->config);
    if (rc == 0) {
        int count = dmsai_port_get_instance_count();
        if (count < 0) rc = count;
        else if (context->config.instance >= count) rc = -ENODEV;
    }
    if (rc == 0) rc = dmsai_port_init(&context->config);
    if (rc != 0) {
        DMOD_LOG_ERROR("dmsai: configuration failed (%d)\n", rc);
        Dmod_Free(context);
        return NULL;
    }
    context->frame_bytes = dmsai_frame_bytes(&context->config);
    context->magic = DMSAI_CONTEXT_MAGIC;
    memset(dev_num, 0, sizeof(*dev_num));
    dev_num->flags = DMDRVI_NUM_MAJOR;
    dev_num->major = context->config.instance;
    __atomic_fetch_add(&active_contexts, 1U, __ATOMIC_RELEASE);
    return context;
}

/**
 * @brief Stop the stream and release the hardware owned by a dmdevfs node.
 * @param context Context returned by dmdrvi_create().
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, void, _free,
    ( dmdrvi_context_t context ))
{
    if (!dmsai_context_valid(context)) return;
    dmsai_port_stop(context->config.instance);
    int rc = dmsai_port_deinit(context->config.instance);
    if (rc != 0) {
        DMOD_LOG_ERROR("dmsai: failed to release controller %u (%d)\n",
                       context->config.instance, rc);
        return;
    }
    context->magic = 0;
    __atomic_fetch_sub(&active_contexts, 1U, __ATOMIC_RELEASE);
    Dmod_Free(context);
}

/**
 * @brief Open one SAI node exclusively with per-handle I/O timeout state.
 * @param context Driver context for this node.
 * @param flags Read, write or read/write access mode.
 * @param dev_num Number of the selected node.
 * @return New handle, or NULL on invalid mode, number or concurrent open.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, void *, _open,
    ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t *dev_num ))
{
    if (!dmsai_context_valid(context) || !dev_num ||
        dev_num->major != context->config.instance) return NULL;
    bool readable, writable;
    if (decode_access(flags, &readable, &writable) != 0) return NULL;
    bool expected = false;
    if (!__atomic_compare_exchange_n(&context->opened, &expected, true,
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return NULL;
    dmsai_handle_t *handle = Dmod_Malloc(sizeof(*handle));
    if (!handle) {
        __atomic_store_n(&context->opened, false, __ATOMIC_RELEASE);
        return NULL;
    }
    *handle = (dmsai_handle_t){ .magic = DMSAI_HANDLE_MAGIC,
        .context = context, .can_read = readable, .can_write = writable };
    return handle;
}

/**
 * @brief Stop the stream and release an exclusive open handle.
 * @param context Driver context.
 * @param handle Handle returned by dmdrvi_open().
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, void, _close,
    ( dmdrvi_context_t context, void *handle ))
{
    if (!dmsai_handle_valid(context, handle)) return;
    dmsai_port_stop(context->config.instance);
    dmsai_handle_t *open = handle;
    open->magic = 0;
    Dmod_Free(open);
    __atomic_store_n(&context->opened, false, __ATOMIC_RELEASE);
}

/** @brief Check one byte-stream request before calling the port. */
static int validate_io(dmdrvi_context_t context, void *handle, const void *buffer,
                       size_t size, dmdrvi_offset_t offset, bool read)
{
    if (!dmsai_handle_valid(context, handle)) return -EBADF;
    if (offset < 0 || (!buffer && size)) return -EINVAL;
    if (size > (size_t)INT64_MAX) return -EOVERFLOW;
    if (size % context->frame_bytes) return -EINVAL;
    dmsai_handle_t *open = handle;
    if (read && !open->can_read) return -EBADF;
    if (!read && !open->can_write) return -EBADF;
    if (read && !context->config.receive) return -ENOTSUP;
    if (!read && !context->config.transmit) return -ENOTSUP;
    return 0;
}

/** @brief Drain TX using the timeout owned by this open handle. */
static int drain_tx(dmdrvi_context_t context, dmsai_handle_t *handle)
{
    if (!handle->can_write || !context->config.transmit) return -EBADF;
    return dmsai_port_flush(context->config.instance, handle->timeout_ms);
}

/**
 * @brief Read complete interleaved PCM frames from the selected SAI RX stream.
 * @param context Driver context.
 * @param handle Handle opened for reading.
 * @param buffer Destination for PCM bytes.
 * @param size Requested byte count, a multiple of the PCM frame size.
 * @param offset Ignored for non-negative values; negative values are invalid.
 * @return Number of bytes read or a negative errno value.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, dmdrvi_ssize_t, _read,
    ( dmdrvi_context_t context, void *handle, void *buffer, size_t size,
      dmdrvi_offset_t offset ))
{
    int rc = validate_io(context, handle, buffer, size, offset, true);
    if (rc != 0) return rc;
    if (size == 0) return 0;
    size_t received = 0;
    rc = dmsai_port_read(context->config.instance, buffer, size, &received,
                         ((dmsai_handle_t *)handle)->timeout_ms);
    return received ? (dmdrvi_ssize_t)received : rc;
}

/**
 * @brief Write complete interleaved PCM frames to the selected SAI TX stream.
 * @param context Driver context.
 * @param handle Handle opened for writing.
 * @param buffer Source PCM bytes.
 * @param size Requested byte count, a multiple of the PCM frame size.
 * @param offset Ignored for non-negative values; negative values are invalid.
 * @return Number of bytes accepted or a negative errno value.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, dmdrvi_ssize_t, _write,
    ( dmdrvi_context_t context, void *handle, const void *buffer, size_t size,
      dmdrvi_offset_t offset ))
{
    int rc = validate_io(context, handle, buffer, size, offset, false);
    if (rc != 0) return rc;
    if (size == 0) return 0;
    size_t written = 0;
    rc = dmsai_port_write(context->config.instance, buffer, size, &written,
                          ((dmsai_handle_t *)handle)->timeout_ms);
    return written ? (dmdrvi_ssize_t)written : rc;
}

/**
 * @brief Handle stream state, configuration, status and timeout commands.
 * @param context Driver context.
 * @param handle Open device handle.
 * @param command DMSAI_IOCTL_* command number.
 * @param arg Command-specific input or output pointer.
 * @return 0 or negative errno; -ENOTTY for unknown commands.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, int, _ioctl,
    ( dmdrvi_context_t context, void *handle, int command, void *arg ))
{
    if (!dmsai_handle_valid(context, handle)) return -EBADF;
    dmsai_handle_t *open = handle;
    switch (command) {
    case DMSAI_IOCTL_GET_CONFIG:
        if (!arg) return -EINVAL;
        *(dmsai_config_t *)arg = context->config;
        return 0;
    case DMSAI_IOCTL_GET_STATUS:
        return arg ? dmsai_port_get_status(context->config.instance, arg) : -EINVAL;
    case DMSAI_IOCTL_GET_IO_TIMEOUT:
        if (!arg) return -EINVAL;
        *(uint32_t *)arg = open->timeout_ms;
        return 0;
    case DMSAI_IOCTL_SET_IO_TIMEOUT:
        if (!arg) return -EINVAL;
        open->timeout_ms = *(const uint32_t *)arg;
        return 0;
    case DMSAI_IOCTL_START: {
        dmsai_status_t status;
        int rc = dmsai_port_get_status(context->config.instance, &status);
        if (rc != 0) return rc;
        return status.running ? -EBUSY : dmsai_port_start(context->config.instance);
    }
    case DMSAI_IOCTL_STOP:
        return dmsai_port_stop(context->config.instance);
    case DMSAI_IOCTL_DRAIN:
        return drain_tx(context, open);
    default:
        return -ENOTTY;
    }
}

/**
 * @brief Wait for queued TX frames to reach the SAI output.
 * @param context Driver context.
 * @param handle Open device handle.
 * @return 0 when drained, or a negative errno on timeout or stop.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, int, _flush,
    ( dmdrvi_context_t context, void *handle ))
{
    if (!dmsai_handle_valid(context, handle)) return -EBADF;
    dmsai_handle_t *open = handle;
    if (!open->can_write || !context->config.transmit) return 0;
    return drain_tx(context, open);
}

/**
 * @brief Report this SAI node as a zero-length character stream.
 * @param context Driver context.
 * @param path dmdevfs node path, unused after node selection.
 * @param stat Receives the size and configured access permissions.
 * @return 0 on success or -EINVAL for an invalid context or output.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, int, _stat,
    ( dmdrvi_context_t context, const char *path, dmdrvi_stat_t *stat ))
{
    (void)path;
    if (!dmsai_context_valid(context) || !stat) return -EINVAL;
    stat->size = 0;
    stat->mode = (context->config.receive ? 0444U : 0U) |
                 (context->config.transmit ? 0222U : 0U);
    return 0;
}
