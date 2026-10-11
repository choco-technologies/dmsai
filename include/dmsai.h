#ifndef DMSAI_H
#define DMSAI_H

#include "dmsai_types.h"

/**
 * @brief SAI device contract exposed through dmdevfs and the dmdrvi DIF.
 *
 * This driver has no separate Built-in Module API. A dmdevfs INI section with
 * `driver_name=dmsai` creates a `/dev/dmsaiN` node. Clients open that path,
 * use the standard DMDRVI_IOCTL_AUDIO_* stream commands (dmdrvi_ioctl.h) to control the
 * stream and transfer PCM bytes with the normal file read/write calls. See docs/api-reference.md.
 */

#endif /* DMSAI_H */
