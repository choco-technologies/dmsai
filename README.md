# dmsai

Proposed, architecture-independent SAI PCM transport API for DMOD. This PR
defines the API and buildable stubs only. All stream and port operations return
`-ENOSYS`; no audio peripheral, DMA engine, clocks, pins or codec are configured.

## Modules and port layout

`dmsai` exposes the application API in `include/dmsai.h` and owns future
configuration checks, stream lifetime and status accounting. `dmsai_port`
exposes the hardware boundary in `include/dmsai_port.h`. Both are independently
loadable DMOD modules; the core depends on the port interface.

The selected `DMOD_CPU_FAMILY` supplies `src/port/<family>/config.cmake` and a
small `port.c` for module lifecycle and future interrupt routing. The STM32F7
family selects `src/port/stm32_common/common.c` for port API definitions, so
register programming, clocks and DMA logic can later be shared with other
STM32 families. A different architecture can select its own common sources
through `DMSAI_PORT_COMMON_SOURCES` without changing the public headers.

## Public API

Include `dmsai.h`. Each function returns zero on success or a negative errno
value. Contexts are opaque. A caller must serialize lifecycle operations on
the same context. The API does not program a codec or board pin routing.

| Function | Intended operation |
|---|---|
| `dmsai_create` | Validate configuration and reserve one stream |
| `dmsai_start` | Start circular PCM transfer and optionally deliver events |
| `dmsai_get_status` | Read actual sample rate, running state and event counters |
| `dmsai_stop` | Stop transfers and release the borrowed PCM buffers |
| `dmsai_destroy` | Release the stream context and hardware resources |

`dmsai_config_t` selects a zero-based peripheral instance, clock master/slave
role, I2S or TDM framing, requested frame rate, allowed clock error in ppm,
sample width, slot width/count, active slot mask, and enabled TX/RX directions.
The port will decide which combinations its target supports and report a
negative errno for unsupported configurations. For I2S, use two slots and
mask `0x3`. `sample_bits` describes valid bits; a 24-bit sample is stored in
a 32-bit buffer element with its valid bits in the low part.

`elements` in `dmsai_start` counts samples from active slots in **each**
non-NULL buffer. Both halves of each circular buffer must hold complete frames,
so use a multiple of twice the number of active slots. The callback reports
which half is ready for refill or consumption and runs in interrupt context.
PCM memory must be DMA-accessible for the chosen port and remain valid until
`dmsai_stop` succeeds.

### Example: stereo transmit

This illustrates the proposed call sequence. With the current stubs,
`dmsai_create` returns `-ENOSYS`, so the transfer does not start.

```c
#include "dmsai.h"

static void audio_event(dmsai_context_t stream, dmsai_direction_t direction,
                        dmsai_event_t event, void *user)
{
    /* Notify a worker to refill the completed TX half. */
    (void)stream;
    (void)direction;
    (void)event;
    (void)user;
}

int start_audio(uint16_t *dma_tx, size_t samples)
{
    dmsai_config_t config = {
        .instance = 0,
        .role = dmsai_clock_master,
        .format = dmsai_format_i2s,
        .sample_rate_hz = 48000,
        .tolerance_ppm = 500,
        .sample_bits = 16,
        .slot_bits = 16,
        .slot_count = 2,
        .active_slots = 0x3,
        .transmit = true,
        .receive = false,
    };
    dmsai_context_t stream;
    int result = dmsai_create(&config, &stream);
    if (result != 0)
        return result;

    result = dmsai_start(stream, dma_tx, NULL, samples, audio_event, NULL);
    if (result == 0)
    {
        dmsai_status_t status;
        result = dmsai_get_status(stream, &status);
        if (result == 0)
            result = dmsai_stop(stream);
    }
    int release_result = dmsai_destroy(stream);
    return result != 0 ? result : release_result;
}
```

For full duplex, set both `transmit` and `receive`, supply separate TX/RX
buffers of the same `elements` length, and use the callback's `direction` to
choose which half to process. For RX-only, set `transmit = false`, pass `NULL`
for TX and enable `receive`. Port support for RX-only and clock slave mode is
target-dependent.

## Build

On a host with the DMOD STM32F7 toolchain installed:

```sh
cmake -S . -B build -DDMOD_CPU_FAMILY=stm32f7 -DDMOD_DIR=/path/to/dmod
cmake --build build
```

See [API reference](docs/api-reference.md) for parameter and error details.
