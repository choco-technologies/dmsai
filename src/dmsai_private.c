#include "dmsai_private.h"

// =======================================================================================
//                  FUNCTIONS
// =======================================================================================

/**
 * @brief checks if the context is valid
 * 
 * The function checks if the given context is valid. 
 * 
 * @param context           context of the driver
 * 
 * @return true if the context is valid
 */
bool is_context_valid( const dmsai_t* context )
{
    return context != NULL && context->magic == DMSAI_MAGIC;
}