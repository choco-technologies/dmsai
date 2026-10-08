#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmdrvi.h"
#include "dmsai.h"
#include "dmsai_ioctl.h"
#include <errno.h>

/**
 * @brief Initialize the driver module without touching SAI hardware.
 * @param config Loader configuration, unused by this stub.
 * @return 0.
 */
int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    Dmod_Printf("dmsai driver interface stub loaded\n");
    return 0;
}

/**
 * @brief Deinitialize the driver module.
 * @return 0; the stub holds no device resources.
 */
int dmod_deinit(void)
{
    return 0;
}

/**
 * @brief Create a dmdevfs driver instance from the selected INI section.
 * @param config dmini context restricted to this device's section by dmdevfs.
 * @param dev_num Receives the major number on success.
 * @return Driver context on success; this stub returns NULL and leaves dev_num unchanged.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, dmdrvi_context_t, _create,
    ( dmini_context_t config, dmdrvi_dev_num_t *dev_num ))
{
    (void)config;
    (void)dev_num;
    return NULL;
}

/**
 * @brief Release a driver instance created by dmdrvi_create().
 * @param context Driver context; ignored by this stub.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, void, _free,
    ( dmdrvi_context_t context ))
{
    (void)context;
}

/**
 * @brief Open one dmdevfs SAI node with the requested access mode.
 * @param context Driver context.
 * @param flags DMDRVI_O_RDONLY, DMDRVI_O_WRONLY or DMDRVI_O_RDWR.
 * @param dev_num Number identifying the SAI controller.
 * @return Exclusive file handle on success, NULL if already open; this stub
 * returns NULL for every call.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, void *, _open,
    ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t *dev_num ))
{
    (void)context;
    (void)flags;
    (void)dev_num;
    return NULL;
}

/**
 * @brief Close a handle opened through dmdevfs.
 * @param context Driver context.
 * @param handle Device handle; ignored by this stub.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, void, _close,
    ( dmdrvi_context_t context, void *handle ))
{
    (void)context;
    (void)handle;
}

/**
 * @brief Read complete PCM frames from RX.
 * @param context Driver context.
 * @param handle Device handle opened for reading.
 * @param buffer Destination for packed PCM bytes.
 * @param size Requested byte count; must be a multiple of frame size.
 * @param offset Ignored for this non-seekable device; must be non-negative.
 * @return Bytes read or negative errno; this stub returns -ENOSYS.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, dmdrvi_ssize_t, _read,
    ( dmdrvi_context_t context, void *handle, void *buffer, size_t size,
      dmdrvi_offset_t offset ))
{
    (void)context;
    (void)handle;
    (void)buffer;
    (void)size;
    (void)offset;
    return -ENOSYS;
}

/**
 * @brief Write complete PCM frames to TX.
 * @param context Driver context.
 * @param handle Device handle opened for writing.
 * @param buffer Source of packed PCM bytes.
 * @param size Requested byte count; must be a multiple of frame size.
 * @param offset Ignored for this non-seekable device; must be non-negative.
 * @return Bytes accepted or negative errno; this stub returns -ENOSYS.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, dmdrvi_ssize_t, _write,
    ( dmdrvi_context_t context, void *handle, const void *buffer, size_t size,
      dmdrvi_offset_t offset ))
{
    (void)context;
    (void)handle;
    (void)buffer;
    (void)size;
    (void)offset;
    return -ENOSYS;
}

/**
 * @brief Dispatch DMSAI_IOCTL_* controls on an open device.
 * @param context Driver context.
 * @param handle Device handle.
 * @param command IOCTL number; unknown commands will return -ENOTTY.
 * @param arg Command-specific argument or NULL.
 * @return 0 or negative errno; this stub returns -ENOSYS for every command.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, int, _ioctl,
    ( dmdrvi_context_t context, void *handle, int command, void *arg ))
{
    (void)context;
    (void)handle;
    (void)command;
    (void)arg;
    return -ENOSYS;
}

/**
 * @brief Wait until previously written PCM has reached the output.
 * @param context Driver context.
 * @param handle Device handle.
 * @return 0 or negative errno; this stub returns -ENOSYS.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, int, _flush,
    ( dmdrvi_context_t context, void *handle ))
{
    (void)context;
    (void)handle;
    return -ENOSYS;
}

/**
 * @brief Report non-seekable character-device metadata.
 * @param context Driver context.
 * @param path dmdevfs path to the SAI node.
 * @param stat Receives size zero and access mode on success.
 * @return 0 or negative errno; this stub returns -ENOSYS.
 */
dmod_dmdrvi_dif_api_declaration(2.0, dmsai, int, _stat,
    ( dmdrvi_context_t context, const char *path, dmdrvi_stat_t *stat ))
{
    (void)context;
    (void)path;
    (void)stat;
    return -ENOSYS;
}
