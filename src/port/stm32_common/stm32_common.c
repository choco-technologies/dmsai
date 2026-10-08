#define DMOD_ENABLE_REGISTRATION ON
#include "dmsai_port.h"
#include <errno.h>

/* All port API definitions live in the shared STM32 source. Family port.c
 * files are reserved for module lifecycle, family descriptors and IRQ glue. */
/** @copydoc dmsai_port_get_instance_count */
dmod_dmsai_port_api_declaration(1.0, int, _get_instance_count, ( void ))
{
    return -ENOSYS;
}

/** @copydoc dmsai_port_init */
dmod_dmsai_port_api_declaration(1.0, int, _init,
    ( const dmsai_config_t *config ))
{
    (void)config;
    return -ENOSYS;
}

/** @copydoc dmsai_port_deinit */
dmod_dmsai_port_api_declaration(1.0, int, _deinit,
    ( dmsai_instance_t instance ))
{
    (void)instance;
    return -ENOSYS;
}

/** @copydoc dmsai_port_start */
dmod_dmsai_port_api_declaration(1.0, int, _start,
    ( dmsai_instance_t instance ))
{
    (void)instance;
    return -ENOSYS;
}

/** @copydoc dmsai_port_stop */
dmod_dmsai_port_api_declaration(1.0, int, _stop,
    ( dmsai_instance_t instance ))
{
    (void)instance;
    return -ENOSYS;
}

/** @copydoc dmsai_port_read */
dmod_dmsai_port_api_declaration(1.0, int, _read,
    ( dmsai_instance_t instance, void *buffer, size_t size, size_t *received,
      uint32_t timeout_ms ))
{
    (void)instance;
    (void)buffer;
    (void)size;
    (void)received;
    (void)timeout_ms;
    return -ENOSYS;
}

/** @copydoc dmsai_port_write */
dmod_dmsai_port_api_declaration(1.0, int, _write,
    ( dmsai_instance_t instance, const void *buffer, size_t size,
      size_t *written, uint32_t timeout_ms ))
{
    (void)instance;
    (void)buffer;
    (void)size;
    (void)written;
    (void)timeout_ms;
    return -ENOSYS;
}

/** @copydoc dmsai_port_get_status */
dmod_dmsai_port_api_declaration(1.0, int, _get_status,
    ( dmsai_instance_t instance, dmsai_status_t *status ))
{
    (void)instance;
    (void)status;
    return -ENOSYS;
}
