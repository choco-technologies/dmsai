#include "stm32_sai_dma.h"
#include "dmdma_lease.h"
#include "dmheap.h"
#include "dmod.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#define DMA_HALF_FRAMES 64U
#define DMA_QUEUE_FRAMES 256U
#define DMA_ALIGNMENT 32U

typedef struct {
    struct stm32_sai_dma *owner;
    bool receive;
} dma_lane_t;

struct stm32_sai_dma {
    dmdma_lease_t tx_lease;
    dmdma_lease_t rx_lease;
    dmheap_context_t *heap;
    uint32_t *tx_dma;
    uint32_t *rx_dma;
    uint32_t *tx_queue;
    uint32_t *rx_queue;
    dmsai_status_t *status;
    dma_lane_t tx_lane;
    dma_lane_t rx_lane;
    uintptr_t tx_dr;
    uintptr_t rx_dr;
    stm32_sai_dma_route_t route;
    dmsai_pcm_format_t format;
    uint32_t half_words;
    uint32_t queue_words;
    uint32_t tx_head;
    uint32_t tx_tail;
    uint32_t rx_head;
    uint32_t rx_tail;
    uint8_t frame_words;
    uint8_t sample_bytes;
    bool transmit;
    bool receive;
    bool active;
    bool fault;
    bool tx_has_data;
};

/** @brief Decode one little-endian PCM sample into a DMA word. */
static uint32_t from_pcm(const uint8_t *source, uint8_t width,
                         dmsai_pcm_format_t format)
{
    uint32_t sample = 0;
    for (uint8_t i = 0; i < width; ++i)
        sample |= (uint32_t)source[i] << (i * 8U);
    return format == dmsai_pcm_s24_in32_le ? sample & 0xFFFFFFU : sample;
}

/** @brief Encode one DMA sample as little-endian PCM. */
static void to_pcm(uint8_t *destination, uint8_t width, uint32_t sample,
                   dmsai_pcm_format_t format)
{
    if (format == dmsai_pcm_s24_in32_le && (sample & 0x800000U))
        sample |= 0xFF000000U;
    for (uint8_t i = 0; i < width; ++i)
        destination[i] = (uint8_t)(sample >> (i * 8U));
}

/** @brief Refill the completed TX half from queued whole frames or silence. */
static void refill_tx(struct stm32_sai_dma *dma, unsigned half)
{
    uint32_t *destination = dma->tx_dma + half * dma->half_words;
    bool underrun = false;
    for (uint32_t i = 0; i < DMA_HALF_FRAMES; ++i) {
        uint32_t tail = dma->tx_tail;
        uint32_t head = __atomic_load_n(&dma->tx_head, __ATOMIC_ACQUIRE);
        uint32_t *frame = destination + i * dma->frame_words;
        if (dma->transmit && head - tail >= dma->frame_words) {
            uint32_t start = tail % dma->queue_words;
            memcpy(frame, dma->tx_queue + start,
                   dma->frame_words * sizeof(uint32_t));
            __atomic_store_n(&dma->tx_tail, tail + dma->frame_words,
                             __ATOMIC_RELEASE);
        } else {
            memset(frame, 0, dma->frame_words * sizeof(uint32_t));
            if (dma->transmit &&
                __atomic_load_n(&dma->tx_has_data, __ATOMIC_ACQUIRE))
                underrun = true;
        }
    }
    if (underrun)
        __atomic_fetch_add(&dma->status->tx_underruns, 1U, __ATOMIC_RELAXED);
}

/** @brief Move a completed RX half into the software queue in whole frames. */
static void collect_rx(struct stm32_sai_dma *dma, unsigned half)
{
    const uint32_t *source = dma->rx_dma + half * dma->half_words;
    bool overrun = false;
    for (uint32_t i = 0; i < DMA_HALF_FRAMES; ++i) {
        uint32_t head = dma->rx_head;
        uint32_t tail = __atomic_load_n(&dma->rx_tail, __ATOMIC_ACQUIRE);
        if (head - tail + dma->frame_words <= dma->queue_words) {
            uint32_t start = head % dma->queue_words;
            memcpy(dma->rx_queue + start, source + i * dma->frame_words,
                   dma->frame_words * sizeof(uint32_t));
            __atomic_store_n(&dma->rx_head, head + dma->frame_words,
                             __ATOMIC_RELEASE);
        } else {
            overrun = true;
        }
    }
    if (overrun)
        __atomic_fetch_add(&dma->status->rx_overruns, 1U, __ATOMIC_RELAXED);
}

/** @brief Handle DMA half and full buffer interrupts without touching user memory. */
static void dma_event(dmdma_lease_t lease, dmdma_event_t event, void *user_ptr)
{
    (void)lease;
    dma_lane_t *lane = user_ptr;
    struct stm32_sai_dma *dma = lane->owner;
    if (!__atomic_load_n(&dma->active, __ATOMIC_ACQUIRE)) return;
    if (event & (dmdma_event_error | dmdma_event_timeout)) {
        __atomic_store_n(&dma->fault, true, __ATOMIC_RELEASE);
        __atomic_fetch_add(&dma->status->transfer_errors, 1U, __ATOMIC_RELAXED);
        return;
    }
    if (event & dmdma_event_half_complete) {
        if (lane->receive) collect_rx(dma, 0);
        else refill_tx(dma, 0);
    }
    if (event & dmdma_event_complete) {
        if (lane->receive) collect_rx(dma, 1);
        else refill_tx(dma, 1);
    }
}

/** @brief Allocate a circular buffer from the firmware's uncached DMA heap. */
static uint32_t *alloc_dma_buffer(struct stm32_sai_dma *dma)
{
    return dmheap_aligned_alloc(dma->heap, DMA_ALIGNMENT,
        2U * dma->half_words * sizeof(uint32_t), "dmsai_port");
}

/** @brief Reserve one route and register its interrupt callback. */
static int reserve_lane(struct stm32_sai_dma *dma, bool receive)
{
    dmdma_lease_t *lease = receive ? &dma->rx_lease : &dma->tx_lease;
    uint8_t stream = receive ? dma->route.rx_stream : dma->route.tx_stream;
    dma_lane_t *lane = receive ? &dma->rx_lane : &dma->tx_lane;
    *lease = dmdma_lease_acquire(dma->route.controller, stream);
    if (!*lease) return -EBUSY;
    lane->owner = dma;
    lane->receive = receive;
    return dmdma_lease_set_callback(*lease, dma_event, lane) == 0 ? 0 : -EIO;
}

/** @copydoc stm32_sai_dma_create */
int stm32_sai_dma_create(stm32_sai_dma_t **result,
    const stm32_sai_dma_route_t *route, uintptr_t tx_dr, uintptr_t rx_dr,
    const dmsai_config_t *config, dmsai_status_t *status)
{
    if (!result || !route || !config || !status) return -EINVAL;
    *result = NULL;
    if (route->tx_stream == UINT8_MAX) return -ENOTSUP;
    stm32_sai_dma_t *dma = Dmod_Malloc(sizeof(*dma));
    if (!dma) return -ENOMEM;
    memset(dma, 0, sizeof(*dma));
    dma->heap = dmheap_get_context_by_name("dma");
    dma->route = *route;
    dma->tx_dr = tx_dr;
    dma->rx_dr = rx_dr;
    dma->status = status;
    dma->format = config->pcm_format;
    dma->frame_words = (uint8_t)__builtin_popcount(config->active_slots);
    dma->sample_bytes = config->slot_bits / 8U;
    dma->half_words = DMA_HALF_FRAMES * dma->frame_words;
    dma->queue_words = DMA_QUEUE_FRAMES * dma->frame_words;
    dma->transmit = config->transmit;
    dma->receive = config->receive;
    int rc = dma->heap ? 0 : -ENODEV;
    if (rc == 0) dma->tx_dma = alloc_dma_buffer(dma);
    if (rc == 0 && !dma->tx_dma) rc = -ENOMEM;
    if (rc == 0 && dma->receive) dma->rx_dma = alloc_dma_buffer(dma);
    if (rc == 0 && dma->receive && !dma->rx_dma) rc = -ENOMEM;
    if (rc == 0 && dma->transmit)
        dma->tx_queue = Dmod_Malloc(dma->queue_words * sizeof(uint32_t));
    if (rc == 0 && dma->transmit && !dma->tx_queue) rc = -ENOMEM;
    if (rc == 0 && dma->receive)
        dma->rx_queue = Dmod_Malloc(dma->queue_words * sizeof(uint32_t));
    if (rc == 0 && dma->receive && !dma->rx_queue) rc = -ENOMEM;
    if (rc == 0) rc = reserve_lane(dma, false);
    if (rc == 0 && dma->receive) rc = reserve_lane(dma, true);
    if (rc != 0) { stm32_sai_dma_destroy(dma); return rc; }
    *result = dma;
    return 0;
}

/** @copydoc stm32_sai_dma_destroy */
void stm32_sai_dma_destroy(stm32_sai_dma_t *dma)
{
    if (!dma) return;
    stm32_sai_dma_stop(dma);
    if (dma->rx_lease) dmdma_lease_release(dma->rx_lease);
    if (dma->tx_lease) dmdma_lease_release(dma->tx_lease);
    if (dma->rx_dma) dmheap_free(dma->heap, dma->rx_dma, true);
    if (dma->tx_dma) dmheap_free(dma->heap, dma->tx_dma, true);
    if (dma->rx_queue) Dmod_Free(dma->rx_queue);
    if (dma->tx_queue) Dmod_Free(dma->tx_queue);
    Dmod_Free(dma);
}

/** @brief Program one circular DMA stream to or from a SAI data register. */
static int start_lane(stm32_sai_dma_t *dma, bool receive)
{
    void *dr = (void *)(receive ? dma->rx_dr : dma->tx_dr);
    uint32_t *buffer = receive ? dma->rx_dma : dma->tx_dma;
    dmdma_transfer_config_t cfg = {
        .direction = receive ? dmdma_direction_peripheral_to_memory
                             : dmdma_direction_memory_to_peripheral,
        .request = receive ? dma->route.rx_request : dma->route.tx_request,
        .source_address = receive ? dr : buffer,
        .destination_address = receive ? buffer : dr,
        .source_width = dmdma_data_width_word,
        .destination_width = dmdma_data_width_word,
        .source_increment = !receive,
        .destination_increment = receive,
        .circular = true,
        .priority = dmdma_priority_high,
        .element_count = 2U * dma->half_words,
        .timeout_ms = 0,
    };
    return dmdma_lease_start(receive ? dma->rx_lease : dma->tx_lease, &cfg);
}

/** @copydoc stm32_sai_dma_start */
int stm32_sai_dma_start(stm32_sai_dma_t *dma)
{
    if (!dma) return -ENODEV;
    memset(dma->tx_dma, 0, 2U * dma->half_words * sizeof(uint32_t));
    if (dma->receive)
        memset(dma->rx_dma, 0, 2U * dma->half_words * sizeof(uint32_t));
    dma->tx_head = dma->tx_tail = dma->rx_head = dma->rx_tail = 0;
    dma->tx_has_data = false;
    __atomic_store_n(&dma->fault, false, __ATOMIC_RELEASE);
    __atomic_store_n(&dma->active, true, __ATOMIC_RELEASE);
    int rc = dma->receive ? start_lane(dma, true) : 0;
    if (rc == 0) rc = start_lane(dma, false);
    if (rc != 0) stm32_sai_dma_stop(dma);
    return rc;
}

/** @copydoc stm32_sai_dma_stop */
void stm32_sai_dma_stop(stm32_sai_dma_t *dma)
{
    if (!dma) return;
    __atomic_store_n(&dma->active, false, __ATOMIC_RELEASE);
    if (dma->tx_lease) dmdma_lease_abort(dma->tx_lease);
    if (dma->rx_lease) dmdma_lease_abort(dma->rx_lease);
}

/** @brief Return a transfer error or wait for one queue change. */
static int wait_queue(stm32_sai_dma_t *dma, Dmod_Timestamp_t started,
                      uint32_t timeout_ms)
{
    if (__atomic_load_n(&dma->fault, __ATOMIC_ACQUIRE)) return -EIO;
    if (!__atomic_load_n(&dma->active, __ATOMIC_ACQUIRE)) return -ECANCELED;
    if (timeout_ms && Dmod_GetUptime() - started >= timeout_ms)
        return -ETIMEDOUT;
    Dmod_ThreadSleep(1);
    return 0;
}

/** @copydoc stm32_sai_dma_write */
int stm32_sai_dma_write(stm32_sai_dma_t *dma, const void *buffer,
    size_t size, size_t *written, uint32_t timeout_ms)
{
    *written = 0;
    uint32_t frame_bytes = dma->frame_words * dma->sample_bytes;
    Dmod_Timestamp_t started = Dmod_GetUptime();
    int rc = 0;
    while (*written < size) {
        if (__atomic_load_n(&dma->fault, __ATOMIC_ACQUIRE)) { rc = -EIO; break; }
        if (!__atomic_load_n(&dma->active, __ATOMIC_ACQUIRE)) {
            rc = -ECANCELED; break;
        }
        uint32_t head = dma->tx_head;
        uint32_t tail = __atomic_load_n(&dma->tx_tail, __ATOMIC_ACQUIRE);
        if (head - tail + dma->frame_words <= dma->queue_words) {
            uint32_t start = head % dma->queue_words;
            const uint8_t *pcm = (const uint8_t *)buffer + *written;
            for (uint8_t i = 0; i < dma->frame_words; ++i)
                dma->tx_queue[start + i] = from_pcm(pcm + i * dma->sample_bytes,
                    dma->sample_bytes, dma->format);
            __atomic_store_n(&dma->tx_head, head + dma->frame_words,
                             __ATOMIC_RELEASE);
            __atomic_store_n(&dma->tx_has_data, true, __ATOMIC_RELEASE);
            *written += frame_bytes;
        } else if ((rc = wait_queue(dma, started, timeout_ms)) != 0) break;
    }
    return *written ? 0 : rc;
}

/** @copydoc stm32_sai_dma_read */
int stm32_sai_dma_read(stm32_sai_dma_t *dma, void *buffer,
    size_t size, size_t *received, uint32_t timeout_ms)
{
    *received = 0;
    uint32_t frame_bytes = dma->frame_words * dma->sample_bytes;
    Dmod_Timestamp_t started = Dmod_GetUptime();
    int rc = 0;
    while (*received < size) {
        if (__atomic_load_n(&dma->fault, __ATOMIC_ACQUIRE)) { rc = -EIO; break; }
        if (!__atomic_load_n(&dma->active, __ATOMIC_ACQUIRE)) {
            rc = -ECANCELED; break;
        }
        uint32_t tail = dma->rx_tail;
        uint32_t head = __atomic_load_n(&dma->rx_head, __ATOMIC_ACQUIRE);
        if (head - tail >= dma->frame_words) {
            uint32_t start = tail % dma->queue_words;
            uint8_t *pcm = (uint8_t *)buffer + *received;
            for (uint8_t i = 0; i < dma->frame_words; ++i)
                to_pcm(pcm + i * dma->sample_bytes, dma->sample_bytes,
                    dma->rx_queue[start + i], dma->format);
            __atomic_store_n(&dma->rx_tail, tail + dma->frame_words,
                             __ATOMIC_RELEASE);
            *received += frame_bytes;
        } else if ((rc = wait_queue(dma, started, timeout_ms)) != 0) break;
    }
    return *received ? 0 : rc;
}
