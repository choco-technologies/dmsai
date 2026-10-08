#define DMOD_ENABLE_REGISTRATION ON
#include "dmsai_port.h"
#include <errno.h>

/* Public port entry points live here so another STM32 family only needs a
 * family config and its small port.c. Hardware work is intentionally absent. */
dmod_dmsai_port_api_declaration(1.0, int, _create,
    ( const dmsai_config_t *config, dmsai_port_context_t *context,
      uint32_t *actual_sample_rate_hz ))
{
    (void)config;
    (void)context;
    (void)actual_sample_rate_hz;
    return -ENOSYS;
}

dmod_dmsai_port_api_declaration(1.0, int, _destroy,
    ( dmsai_port_context_t context ))
{
    (void)context;
    return -ENOSYS;
}

dmod_dmsai_port_api_declaration(1.0, int, _start,
    ( dmsai_port_context_t context, const void *tx, void *rx, size_t elements,
      dmsai_port_callback_t callback, void *user ))
{
    (void)context;
    (void)tx;
    (void)rx;
    (void)elements;
    (void)callback;
    (void)user;
    return -ENOSYS;
}

dmod_dmsai_port_api_declaration(1.0, int, _stop,
    ( dmsai_port_context_t context ))
{
    (void)context;
    return -ENOSYS;
}
