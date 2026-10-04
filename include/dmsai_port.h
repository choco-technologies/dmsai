#ifndef DMSAI_PORT_H
#define DMSAI_PORT_H
#include "dmod.h"
#include "dmsai_port_defs.h"
#include "dmsai_types.h"
typedef struct dmsai_port_context *dmsai_port_context_t;
typedef void (*dmsai_port_callback_t)(dmsai_direction_t direction, dmsai_event_t event, void *user);
dmod_dmsai_port_api(1.0, int, _create, ( const dmsai_config_t *config, dmsai_port_context_t *context, uint32_t *frequency ) );
dmod_dmsai_port_api(1.0, int, _destroy, ( dmsai_port_context_t context ) );
dmod_dmsai_port_api(1.0, int, _start, ( dmsai_port_context_t context, const void *tx, void *rx, size_t elements, dmsai_port_callback_t callback, void *user ) );
dmod_dmsai_port_api(1.0, int, _stop, ( dmsai_port_context_t context ) );
#endif
