#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmsai.h"
#include <errno.h>

/**
 * @brief Initialize the core module without starting a stream.
 * @param config DMOD loader configuration; unused by this interface stub.
 * @return 0 after the module is loaded.
 */
int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    Dmod_Printf("dmsai API stub loaded\n");
    return 0;
}

/**
 * @brief Deinitialize the core module.
 * @return 0; no resources are allocated by this stub.
 */
int dmod_deinit(void)
{
    return 0;
}

dmod_dmsai_api_declaration(1.0, int, _create,
    ( const dmsai_config_t *config, dmsai_context_t *context ))
{
    (void)config;
    (void)context;
    return -ENOSYS;
}

dmod_dmsai_api_declaration(1.0, int, _destroy, ( dmsai_context_t context ))
{
    (void)context;
    return -ENOSYS;
}

dmod_dmsai_api_declaration(1.0, int, _start,
    ( dmsai_context_t context, const void *tx, void *rx, size_t elements,
      dmsai_callback_t callback, void *user ))
{
    (void)context;
    (void)tx;
    (void)rx;
    (void)elements;
    (void)callback;
    (void)user;
    return -ENOSYS;
}

dmod_dmsai_api_declaration(1.0, int, _stop, ( dmsai_context_t context ))
{
    (void)context;
    return -ENOSYS;
}

dmod_dmsai_api_declaration(1.0, int, _get_status,
    ( dmsai_context_t context, dmsai_status_t *status ))
{
    (void)context;
    (void)status;
    return -ENOSYS;
}
