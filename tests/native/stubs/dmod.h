#ifndef DMSAI_TEST_DMOD_H
#define DMSAI_TEST_DMOD_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
typedef struct { int unused; } Dmod_Config_t;
#define Dmod_Malloc malloc
#define Dmod_Free free
void Dmod_EnterCritical(void);
void Dmod_ExitCritical(void);
#endif
