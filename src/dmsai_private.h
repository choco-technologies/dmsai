#ifndef DMSAI_PRIVATE_H
#define DMSAI_PRIVATE_H

#include "dmdrvi.h"
#include "dmsai_types.h"

#define DMSAI_CONTEXT_MAGIC 0x44534149U
#define DMSAI_HANDLE_MAGIC  0x53414948U

struct dmdrvi_context
{
    uint32_t magic;
    dmsai_config_t config;
    size_t frame_bytes;
    bool opened;
};

typedef struct
{
    uint32_t magic;
    dmdrvi_context_t context;
    uint32_t timeout_ms;
    bool can_read;
    bool can_write;
} dmsai_handle_t;

/** @brief Parse a selected dmdevfs INI section into portable SAI settings. */
int dmsai_parse_config(dmini_context_t ini, dmsai_config_t *config);

/** @brief Return the byte count of one interleaved PCM frame. */
size_t dmsai_frame_bytes(const dmsai_config_t *config);

/** @brief Check an opaque driver context before dereferencing it. */
bool dmsai_context_valid(dmdrvi_context_t context);

/** @brief Check that an open handle belongs to the supplied driver context. */
bool dmsai_handle_valid(dmdrvi_context_t context, const void *handle);

#endif /* DMSAI_PRIVATE_H */
