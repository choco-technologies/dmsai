#ifndef DMSAI_H
#define DMSAI_H

#include "dmsai_types.h"

/**
 * @brief SAI device contract exposed through dmdevfs and the dmdrvi DIF.
 *
 * This driver has no separate Built-in Module API. A dmdevfs INI section with
 * `driver_name=dmsai` creates a `/dev/dmsaiN` node. Clients open that path,
 * use commands from dmsai_ioctl.h to control transfer state and transfer PCM
 * bytes with the normal file read/write calls. See docs/api-reference.md.
 */

#endif /* DMSAI_H */
