#include "dmsai.h"
#include "dmsai_port.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
struct dmsai_port_context { int value; };
static struct dmsai_port_context fake;
static dmsai_port_callback_t handler;
static void *handler_user;
static int create_result, start_result, stop_result;
static unsigned callbacks, creates, destroys;
void Dmod_EnterCritical(void) {}
void Dmod_ExitCritical(void) {}
int dmod_deinit(void);
int dmsai_port_create(const dmsai_config_t *config, dmsai_port_context_t *context, uint32_t *frequency)
{
    (void)config; creates++;
    if (create_result) return create_result;
    *context = &fake; *frequency = 12285714; return 0;
}
int dmsai_port_destroy(dmsai_port_context_t context)
{
    assert(context == &fake);
    if (stop_result) return stop_result;
    handler = NULL; destroys++; return 0;
}
int dmsai_port_start(dmsai_port_context_t context, const void *tx, void *rx, size_t n, dmsai_port_callback_t callback, void *user)
{
    assert(context == &fake && tx && rx && n == 16);
    if (start_result) return start_result;
    handler = callback; handler_user = user; return 0;
}
int dmsai_port_stop(dmsai_port_context_t context)
{
    assert(context == &fake);
    if (stop_result) return stop_result;
    handler = NULL; return 0;
}
static void callback(dmsai_context_t context, dmsai_direction_t direction, dmsai_event_t event, void *user)
{
    assert(context && direction == dmsai_rx && (event & dmsai_event_complete) && user == &fake);
    callbacks++;
}
static dmsai_config_t config = {2, 48000, 500, 16, 16, 4, 5, true};

static void invalid_configs(void)
{
    dmsai_context_t c = NULL;
    assert(dmsai_create(NULL, &c) == -EINVAL);
    assert(dmsai_create(&config, NULL) == -EINVAL);
    dmsai_config_t bad = config;
    bad.sample_bits = 24; assert(dmsai_create(&bad, &c) == -EINVAL);
    bad = config; bad.active_slots = 1; assert(dmsai_create(&bad, &c) == -EINVAL);
    bad = config; bad.slot_count = 3; assert(dmsai_create(&bad, &c) == -EINVAL);
    bad = config; bad.active_slots = 16; assert(dmsai_create(&bad, &c) == -EINVAL);
    bad = config; bad.sample_rate = UINT32_MAX; assert(dmsai_create(&bad, &c) == -EINVAL);
    bad = config; bad.slot_count = 16; bad.slot_bits = 32;
    assert(dmsai_create(&bad, &c) == -EINVAL && !creates);
    create_result = -EBUSY;
    assert(dmsai_create(&config, &c) == -EBUSY && !c);
    create_result = 0;
    assert(dmsai_destroy(NULL) == -EINVAL && dmsai_stop(NULL) == -EINVAL);
}
static void lifecycle(void)
{
    dmsai_context_t c = NULL; dmsai_status_t status;
    uint16_t tx[16] = {0}, rx[16] = {0};
    assert(dmsai_create(&config, &c) == 0 && c);
    assert(dmod_deinit() == -EBUSY);
    assert(dmsai_get_status(c, &status) == 0 && status.actual_sample_rate == 47991);
    assert(dmsai_start(c, tx, rx, 2, callback, &fake) == -EINVAL);
    assert(dmsai_start(c, tx, rx, 65536, callback, &fake) == -EINVAL);
    assert(dmsai_start(c, tx, tx, 16, callback, &fake) == -EINVAL);
    assert(dmsai_start(c, tx, NULL, 16, callback, &fake) == -EINVAL);
    assert(dmsai_start(c, (char *)tx + 1, rx, 16, callback, &fake) == -EINVAL);
    start_result = -EIO;
    assert(dmsai_start(c, tx, rx, 16, callback, &fake) == -EIO);
    assert(dmsai_get_status(c, &status) == 0 && !status.running);
    start_result = 0;
    assert(dmsai_start(c, tx, rx, 16, callback, &fake) == 0);
    assert(dmsai_start(c, tx, rx, 16, NULL, NULL) == -EBUSY);
    handler(dmsai_rx, dmsai_event_half | dmsai_event_complete | dmsai_event_dma_error | dmsai_event_fifo_error, handler_user);
    assert(callbacks == 1 && dmsai_get_status(c, &status) == 0);
    assert(status.half_events[1] == 1 && status.complete_events[1] == 1 && status.dma_errors[1] == 1 && status.fifo_errors[1] == 1);
    stop_result = -ETIMEDOUT;
    assert(dmsai_stop(c) == -ETIMEDOUT && dmsai_destroy(c) == -ETIMEDOUT && !destroys);
    stop_result = 0;
    assert(dmsai_stop(c) == 0 && !handler);
    assert(dmsai_get_status(c, &status) == 0 && !status.running);
    assert(dmsai_stop(c) == 0 && dmsai_destroy(c) == 0 && destroys == 1);
    assert(dmod_deinit() == 0);
}
int main(void) { invalid_configs(); lifecycle(); puts("DMSAI contract: PASS"); }
