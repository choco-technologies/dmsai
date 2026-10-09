#ifndef DMSAI_IOCTL_H
#define DMSAI_IOCTL_H

#include "dmdrvi_ioctl.h"
#include "dmsai_types.h"

/**
 * @brief Read the effective device configuration.
 *
 * @par Argument
 * Output pointer to dmsai_config_t. May be used before START.
 */
#define DMSAI_IOCTL_GET_CONFIG       (DMDRVI_IOCTL_CUSTOM_BASE + 0)

/**
 * @brief Begin PCM transfer after the device is opened.
 *
 * @par Argument
 * NULL. A second START while running returns -EBUSY.
 */
#define DMSAI_IOCTL_START            (DMDRVI_IOCTL_CUSTOM_BASE + 1)

/**
 * @brief Stop PCM transfer and release pending I/O waits.
 *
 * @par Argument
 * NULL. STOP on an idle device succeeds.
 */
#define DMSAI_IOCTL_STOP             (DMDRVI_IOCTL_CUSTOM_BASE + 2)

/**
 * @brief Read transfer state and cumulative error counters.
 *
 * @par Argument
 * Output pointer to dmsai_status_t.
 */
#define DMSAI_IOCTL_GET_STATUS       (DMDRVI_IOCTL_CUSTOM_BASE + 3)

/**
 * @brief Set the current handle's read/write wait limit.
 *
 * @par Argument
 * Input pointer to uint32_t milliseconds. Zero means no limit.
 */
#define DMSAI_IOCTL_SET_IO_TIMEOUT   (DMDRVI_IOCTL_CUSTOM_BASE + 4)

/**
 * @brief Read the current handle's read/write wait limit.
 *
 * @par Argument
 * Output pointer to uint32_t milliseconds.
 */
#define DMSAI_IOCTL_GET_IO_TIMEOUT   (DMDRVI_IOCTL_CUSTOM_BASE + 5)

/**
 * @brief Wait until previously accepted TX frames reach the output.
 *
 * @par Argument
 * NULL. The current handle's I/O timeout applies. STOP cancels the wait.
 */
#define DMSAI_IOCTL_DRAIN            (DMDRVI_IOCTL_CUSTOM_BASE + 6)

#endif /* DMSAI_IOCTL_H */
