#ifndef DMSAI_TYPES_H
#define DMSAI_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct dmsai_context *dmsai_context_t;
typedef enum { dmsai_tx = 0, dmsai_rx = 1 } dmsai_direction_t;
typedef enum
{
    dmsai_event_half = 1, dmsai_event_complete = 2,
    dmsai_event_dma_error = 4, dmsai_event_fifo_error = 8
} dmsai_event_t;
typedef void (*dmsai_callback_t)(dmsai_context_t context, dmsai_direction_t direction,
                                dmsai_event_t event, void *user);
typedef struct
{
    uint8_t instance;       /* STM32F746: 2 (SAI2) */
    uint32_t sample_rate;   /* 8000..96000 Hz */
    uint32_t tolerance_ppm; /* 0 = exact; typically 500 */
    uint8_t sample_bits;    /* 16, 24 or 32 */
    uint8_t slot_bits;      /* 16 or 32, >= sample_bits */
    uint8_t slot_count;     /* even, 2..16; frame <= 256 bits */
    uint16_t active_slots;  /* even number of active slots, nonzero */
    bool receive;          /* reserve synchronous SAI2 block B / RX DMA */
} dmsai_config_t;
typedef struct
{
    uint32_t half_events[2], complete_events[2], dma_errors[2], fifo_errors[2];
    uint32_t actual_sample_rate; /* rounded Hz, kernel / 256 */
    uint32_t kernel_frequency;
    bool running;
} dmsai_status_t;
#endif
