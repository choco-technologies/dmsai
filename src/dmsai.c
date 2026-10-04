#define DMOD_ENABLE_REGISTRATION ON
#include "dmsai.h"
#include "dmsai_port.h"
#include "dmsai_validate.h"
#include <string.h>
#define MAGIC 0x44534149U
struct dmsai_context
{
    uint32_t magic;
    dmsai_config_t config;
    dmsai_port_context_t port;
    dmsai_status_t status;
    dmsai_callback_t callback;
    void *user;
};
static unsigned contexts;
static bool valid(dmsai_context_t c) { return c && c->magic == MAGIC; }
static void event(dmsai_direction_t direction, dmsai_event_t e, void *user)
{
    dmsai_context_t c = user;
    if (!valid(c) || direction > dmsai_rx) return;
    if (e & dmsai_event_half) c->status.half_events[direction]++;
    if (e & dmsai_event_complete) c->status.complete_events[direction]++;
    if (e & dmsai_event_dma_error) c->status.dma_errors[direction]++;
    if (e & dmsai_event_fifo_error) c->status.fifo_errors[direction]++;
    if (c->callback) c->callback(c, direction, e, c->user);
}
int dmod_init(const Dmod_Config_t *config) { (void)config; return 0; }
int dmod_deinit(void) { return contexts ? -EBUSY : 0; }
dmod_dmsai_api_declaration(1.0, int, _create, ( const dmsai_config_t *config, dmsai_context_t *context ) )
{
    if (!context) return -EINVAL;
    int result = dmsai_validate_config(config);
    if (result) return result;
    dmsai_context_t c = Dmod_Malloc(sizeof(*c));
    if (!c) return -ENOMEM;
    memset(c, 0, sizeof(*c));
    c->magic = MAGIC; c->config = *config;
    result = dmsai_port_create(config, &c->port, &c->status.kernel_frequency);
    if (result) { c->magic = 0; Dmod_Free(c); return result; }
    c->status.actual_sample_rate = (c->status.kernel_frequency + 128) / 256;
    contexts++; *context = c;
    return 0;
}
dmod_dmsai_api_declaration(1.0, int, _destroy, ( dmsai_context_t context ) )
{
    if (!valid(context)) return -EINVAL;
    int result = dmsai_port_destroy(context->port);
    if (result) return result;
    context->magic = 0; contexts--; Dmod_Free(context);
    return 0;
}
dmod_dmsai_api_declaration(1.0, int, _start, ( dmsai_context_t context, const void *tx, void *rx, size_t elements, dmsai_callback_t callback, void *user ) )
{
    if (!valid(context)) return -EINVAL;
    if (context->status.running) return -EBUSY;
    int result = dmsai_validate_buffer(&context->config, tx, rx, elements);
    if (result) return result;
    context->callback = callback; context->user = user;
    result = dmsai_port_start(context->port, tx, rx, elements, event, context);
    if (!result) context->status.running = true;
    else { context->callback = NULL; context->user = NULL; }
    return result;
}
dmod_dmsai_api_declaration(1.0, int, _stop, ( dmsai_context_t context ) )
{
    if (!valid(context)) return -EINVAL;
    int result = dmsai_port_stop(context->port);
    if (!result)
    {
        context->status.running = false;
        context->callback = NULL; context->user = NULL;
    }
    return result;
}
dmod_dmsai_api_declaration(1.0, int, _get_status, ( dmsai_context_t context, dmsai_status_t *status ) )
{
    if (!valid(context) || !status) return -EINVAL;
    Dmod_EnterCritical(); *status = context->status; Dmod_ExitCritical();
    return 0;
}
