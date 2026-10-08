#ifndef DMSAI_TYPES_H
#define DMSAI_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Zero-based SAI controller number on the selected target. */
typedef uint8_t dmsai_instance_t;

/** @brief Source of serial bit and frame clocks. */
typedef enum
{
    dmsai_clock_master = 0, /**< The controller generates both clocks. */
    dmsai_clock_slave       /**< The controller receives both clocks. */
} dmsai_clock_role_t;

/** @brief Serial framing of one PCM sample frame. */
typedef enum
{
    dmsai_frame_i2s = 0, /**< Standard two-slot I2S framing. */
    dmsai_frame_tdm      /**< Time-division multiplexed slots. */
} dmsai_frame_format_t;

/** @brief Signed PCM representation in device read/write buffers. */
typedef enum
{
    dmsai_pcm_s16_le = 0,    /**< Signed 16-bit little-endian, 2 bytes. */
    dmsai_pcm_s24_in32_le,  /**< Signed 24-bit in low bits of a 32-bit LE word; upper byte ignored on TX, sign-extended on RX. */
    dmsai_pcm_s32_le        /**< Signed 32-bit little-endian, 4 bytes. */
} dmsai_pcm_format_t;

/**
 * @brief Portable configuration of one SAI controller.
 *
 * The core reads these values from the dmdevfs-selected INI section. The
 * selected port decides whether the combination is supported. Both read and
 * write operate on complete PCM frames made from the selected active slots.
 */
typedef struct
{
    dmsai_instance_t instance;      /**< Zero-based controller index. */
    dmsai_clock_role_t clock_role;  /**< Clock producer or consumer. */
    dmsai_frame_format_t framing;  /**< I2S or TDM framing. */
    dmsai_pcm_format_t pcm_format;  /**< Representation of user PCM samples. */
    uint32_t sample_rate_hz;        /**< Requested frame rate in Hz. */
    uint32_t tolerance_ppm;         /**< Maximum absolute clock error; zero means exact. */
    uint8_t slot_bits;              /**< Width on the wire of each time slot. */
    uint8_t slot_count;             /**< Number of slots in each frame, 1 to 32. */
    uint16_t frame_bits;            /**< Total frame length on the wire, including padding. */
    uint32_t active_slots;          /**< Bit N selects time slot N. */
    bool transmit;                  /**< Enable write access to the TX direction. */
    bool receive;                   /**< Enable read access to the RX direction. */
} dmsai_config_t;

/** @brief Status returned by DMSAI_IOCTL_GET_STATUS. */
typedef struct
{
    uint32_t actual_sample_rate_hz; /**< Frame rate achieved by the port. */
    uint32_t tx_underruns;          /**< Transmit starvation events since creation. */
    uint32_t rx_overruns;           /**< Receive samples lost since creation. */
    uint32_t transfer_errors;       /**< Other DMA or serial transfer failures. */
    bool running;                   /**< True after successful START. */
} dmsai_status_t;

#endif /* DMSAI_TYPES_H */
