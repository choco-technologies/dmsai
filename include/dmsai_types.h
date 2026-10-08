#ifndef DMSAI_TYPES_H
#define DMSAI_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Opaque handle for one configured SAI stream. */
typedef struct dmsai_context *dmsai_context_t;

/** @brief SAI clock ownership for the configured stream. */
typedef enum
{
    dmsai_clock_master, /**< Generate bit clock and frame synchronization. */
    dmsai_clock_slave   /**< Receive clocks from an external source. */
} dmsai_clock_role_t;

/** @brief Serial frame format; availability depends on the port. */
typedef enum
{
    dmsai_format_i2s, /**< Standard two-slot I2S frame. */
    dmsai_format_tdm  /**< Time-division multiplexed frame. */
} dmsai_format_t;

/** @brief Direction associated with a transfer event or status counter. */
typedef enum
{
    dmsai_tx = 0, /**< Samples sent to the serial data output. */
    dmsai_rx = 1  /**< Samples captured from the serial data input. */
} dmsai_direction_t;

/** @brief Bit flags delivered by a transfer callback. */
typedef enum
{
    dmsai_event_half        = 1u << 0, /**< First buffer half is available. */
    dmsai_event_complete    = 1u << 1, /**< Second buffer half is available. */
    dmsai_event_dma_error   = 1u << 2, /**< A DMA transfer failed. */
    dmsai_event_frame_error = 1u << 3  /**< Serial peripheral reported an error. */
} dmsai_event_t;

/**
 * @brief Called when a buffer half completes or a transfer error occurs.
 *
 * The callback runs in interrupt context. It must only notify a worker or
 * perform other bounded interrupt-safe work; it must not call the stream
 * lifecycle functions. `user` is the pointer passed to dmsai_start().
 *
 * @param context Stream which produced the event.
 * @param direction Transfer direction.
 * @param event Bitwise combination of dmsai_event_t flags.
 * @param user Caller-owned callback argument.
 */
typedef void (*dmsai_callback_t)(dmsai_context_t context,
                                  dmsai_direction_t direction,
                                  dmsai_event_t event, void *user);

/** @brief Architecture-independent configuration of one synchronous stream. */
typedef struct
{
    uint8_t instance;            /**< Zero-based peripheral index on the target. */
    dmsai_clock_role_t role;     /**< Whether this endpoint generates clocks. */
    dmsai_format_t format;       /**< Serial framing convention. */
    uint32_t sample_rate_hz;     /**< Requested frame rate in Hz. */
    uint32_t tolerance_ppm;      /**< Maximum clock error; zero requests exact rate. */
    uint8_t sample_bits;         /**< Significant bits per sample (e.g. 16, 24, 32). */
    uint8_t slot_bits;           /**< Bits transmitted per slot; at least sample_bits. */
    uint8_t slot_count;          /**< Slots per frame, from 1 to 32. */
    uint32_t active_slots;       /**< Bit N selects slot N for the packed PCM buffers. */
    bool transmit;               /**< Enable transmit direction. */
    bool receive;                /**< Enable receive direction. */
} dmsai_config_t;

/** @brief Snapshot of a configured stream. Counters accumulate across starts. */
typedef struct
{
    uint32_t half_events[2];     /**< Half-buffer callbacks by dmsai_direction_t. */
    uint32_t complete_events[2]; /**< Full-buffer callbacks by direction. */
    uint32_t dma_errors[2];      /**< DMA faults by direction. */
    uint32_t frame_errors[2];    /**< Serial peripheral faults by direction. */
    uint32_t actual_sample_rate_hz; /**< Frame rate provided by the port. */
    bool running;                /**< True after a successful start. */
} dmsai_status_t;

#endif /* DMSAI_TYPES_H */
