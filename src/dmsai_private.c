#include "dmsai_private.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

/** @brief Read a key from the section selected by dmdevfs or legacy [main]. */
static const char *setting(dmini_context_t ini, const char *key)
{
    const char *value = dmini_get_string(ini, NULL, key, NULL);
    return value ? value : dmini_get_string(ini, "main", key, NULL);
}

/** @brief Parse one unsigned decimal INI value without truncation. */
static int parse_u32(const char *text, uint32_t maximum, uint32_t *result)
{
    if (!text || !*text) return -EINVAL;
    uint32_t value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return -EINVAL;
        uint32_t digit = (uint32_t)(*p - '0');
        if (digit > maximum || value > (maximum - digit) / 10U)
            return -ERANGE;
        value = value * 10U + digit;
    }
    *result = value;
    return 0;
}

/** @brief Parse the on/off representation used by driver INI files. */
static int parse_bool(const char *text, bool *result)
{
    if (!text) return -EINVAL;
    if (!strcmp(text, "on") || !strcmp(text, "true") || !strcmp(text, "1"))
        *result = true;
    else if (!strcmp(text, "off") || !strcmp(text, "false") || !strcmp(text, "0"))
        *result = false;
    else return -EINVAL;
    return 0;
}

/** @brief Parse an enum value from the documented clock/framing strings. */
static int parse_mode(const char *text, const char *first,
                      const char *second, int *result)
{
    if (!text) return -EINVAL;
    if (!strcmp(text, first)) *result = 0;
    else if (!strcmp(text, second)) *result = 1;
    else return -EINVAL;
    return 0;
}

/** @brief Decode sample representation. */
static int parse_format(const char *text, dmsai_pcm_format_t *format)
{
    if (!text) return -EINVAL;
    if (!strcmp(text, "s16_le")) *format = dmsai_pcm_s16_le;
    else if (!strcmp(text, "s24_in32_le")) *format = dmsai_pcm_s24_in32_le;
    else if (!strcmp(text, "s32_le")) *format = dmsai_pcm_s32_le;
    else return -EINVAL;
    return 0;
}

/** @brief Decode frame geometry without narrowing oversized INI numbers. */
static int parse_geometry(dmini_context_t ini, dmsai_config_t *config)
{
    uint32_t value;
    if (parse_u32(setting(ini, "slot_bits"), UINT8_MAX, &value)) return -EINVAL;
    config->slot_bits = (uint8_t)value;
    if (parse_u32(setting(ini, "slot_count"), UINT8_MAX, &value)) return -EINVAL;
    config->slot_count = (uint8_t)value;
    if (parse_u32(setting(ini, "frame_bits"), UINT16_MAX, &value)) return -EINVAL;
    config->frame_bits = (uint16_t)value;
    if (parse_u32(setting(ini, "active_slots"), UINT32_MAX, &config->active_slots)) return -EINVAL;
    return 0;
}

/** @copydoc dmsai_parse_config */
int dmsai_parse_config(dmini_context_t ini, dmsai_config_t *config)
{
    if (!ini || !config) return -EINVAL;
    memset(config, 0, sizeof(*config));
    uint32_t value;
    int mode;
    if (parse_u32(setting(ini, "instance"), UINT8_MAX, &value)) return -EINVAL;
    config->instance = (dmsai_instance_t)value;
    if (parse_mode(setting(ini, "clock_role"), "master", "slave", &mode)) return -EINVAL;
    config->clock_role = (dmsai_clock_role_t)mode;
    if (parse_mode(setting(ini, "framing"), "i2s", "tdm", &mode)) return -EINVAL;
    config->framing = (dmsai_frame_format_t)mode;
    if (parse_format(setting(ini, "pcm_format"), &config->pcm_format)) return -EINVAL;
    if (parse_u32(setting(ini, "sample_rate_hz"), UINT32_MAX, &config->sample_rate_hz)) return -EINVAL;
    const char *tolerance = setting(ini, "tolerance_ppm");
    if (tolerance && parse_u32(tolerance, UINT32_MAX, &config->tolerance_ppm)) return -EINVAL;
    if (parse_geometry(ini, config)) return -EINVAL;
    if (parse_bool(setting(ini, "transmit"), &config->transmit) ||
        parse_bool(setting(ini, "receive"), &config->receive)) return -EINVAL;
    if (!config->sample_rate_hz || !config->slot_count || !config->active_slots ||
        (!config->transmit && !config->receive)) return -EINVAL;
    return 0;
}

/** @copydoc dmsai_frame_bytes */
size_t dmsai_frame_bytes(const dmsai_config_t *config)
{
    return (size_t)__builtin_popcount(config->active_slots) *
        (config->pcm_format == dmsai_pcm_s16_le ? 2U : 4U);
}

/** @copydoc dmsai_context_valid */
bool dmsai_context_valid(dmdrvi_context_t context)
{
    return context && context->magic == DMSAI_CONTEXT_MAGIC;
}

/** @copydoc dmsai_handle_valid */
bool dmsai_handle_valid(dmdrvi_context_t context, const void *handle)
{
    const dmsai_handle_t *open = handle;
    return dmsai_context_valid(context) && open &&
        open->magic == DMSAI_HANDLE_MAGIC && open->context == context;
}
