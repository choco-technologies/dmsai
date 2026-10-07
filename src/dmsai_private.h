#ifndef DMSAI_PRIVATE_H
#define DMSAI_PRIVATE_H

#include "dmsai.h"

// =======================================================================================
//                  DEFINITIONS
// =======================================================================================
#define DMSAI_MAGIC         0x646D7361

// =======================================================================================
//                  TYPES
// =======================================================================================

/* Example internal state - replace with your module's real fields. */
typedef struct dmdrvi_context
{
    uint32_t        magic;  //!< MAGIC number for verification of the structure
} dmsai_t;

// =======================================================================================
//                  FUNCTIONS
// =======================================================================================

extern bool is_context_valid( const dmsai_t* context );

#endif // DMSAI_PRIVATE_H