# dmsai

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmsai/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmsai/actions/workflows/ci.yml)

`dmsai` proposes a PCM SAI device driver for DMOD. It implements the `dmdrvi`
DIF used by `dmdevfs`, following the same device model as `dmeth`: an INI
section selects the driver and each configured controller is intended to appear
as `/dev/dmsaiN`. Applications use ordinary file read/write operations and
driver-specific `ioctl` commands. There is no separate stream Module API.

`dmdevfs` owns device node creation and mounting. The `dmsai` module's DIF
callbacks are still stubs: its `dmdrvi_create()` returns `NULL`, so `dmdevfs`
rejects this driver configuration and does not add a `/dev/dmsaiN` node yet.
The STM32F7 `dmsai_port` now configures SAI hardware and can be exercised
directly by another DMOD module.

## Device contract

The `dmdrvi` DIF supplies `create/free`, `open/close`, `read/write`, `ioctl`,
`flush` and `stat`. The core will parse the active INI section selected by
`dmdevfs`, allocate the device context and expose a major number equal to the
zero-based `instance` (`/dev/dmsai0`, `/dev/dmsai1`, ...). Unknown ioctls are
intended to return `-ENOTTY` when implemented.

`read` and `write` transfer **bytes** of interleaved PCM in complete frames.
Enabled slots are packed in ascending slot order. The sample representation
is explicit: signed 16-bit little-endian, signed 24-bit in the low 24 bits of
a 32-bit little-endian word, or signed 32-bit little-endian. For stereo 16-bit
audio, one frame is four bytes. A request size must be a multiple of frame
size; a successful transfer can return a shorter whole-frame count. The device
is non-seekable. Callers pass normal PCM buffers; the port owns its hardware
resources and transfer mechanism.
An open device has one exclusive handle, so two clients cannot independently
start, stop or change the same stream.

The device-specific commands in `dmsai_ioctl.h` are:

| Command | Argument | Purpose |
|---|---|---|
| `DMSAI_IOCTL_GET_CONFIG` | `dmsai_config_t *` | Read effective configuration |
| `DMSAI_IOCTL_START` | `NULL` | Start configured directions |
| `DMSAI_IOCTL_STOP` | `NULL` | Stop transfer and wake pending I/O |
| `DMSAI_IOCTL_GET_STATUS` | `dmsai_status_t *` | Read rate and error counters |
| `DMSAI_IOCTL_SET_IO_TIMEOUT` | `const uint32_t *` | Set this handle's wait limit in ms; zero means no limit |
| `DMSAI_IOCTL_GET_IO_TIMEOUT` | `uint32_t *` | Read this handle's wait limit |

## Configuration example

`dmdevfs` discovers the section from `driver_name=dmsai` and restricts the
`dmini` context to it before calling the driver. Board pin routing and codec
initialization are configured separately.

```ini
[audio0]
driver_name=dmsai
instance=0
clock_role=master
framing=i2s
pcm_format=s16_le
sample_rate_hz=48000
tolerance_ppm=500
slot_bits=16
slot_count=2
frame_bits=32
active_slots=3
transmit=on
receive=off
```

The port decides which rate, role, framing and directions its target supports.
The core stub does not yet parse this configuration.

Board GPIO mappings and pin-agnostic MCU defaults are in
[configs/README.md](configs/README.md). Select one device section per SAI
controller; the board examples are based on ST's audio BSP pin definitions.

## Usage example

This is the intended DMOD file API sequence once the driver is implemented.
The current stub cannot be opened through `dmdevfs`.

```c
#include "dmod.h"
#include "dmsai.h"
#include "dmsai_ioctl.h"

int play_stereo(const int16_t *pcm, size_t frames)
{
    void *audio = Dmod_FileOpen("/dev/dmsai0", "w");
    if (audio == NULL)
        return -1;

    int result = Dmod_Ioctl(audio, DMSAI_IOCTL_START, NULL);
    if (result == 0)
    {
        size_t bytes = frames * 2u * sizeof(int16_t);
        if (Dmod_FileWrite(pcm, 1, bytes, audio) != bytes)
            result = -1;
        if (result == 0)
            result = Dmod_Ioctl(audio, DMSAI_IOCTL_STOP, NULL);
    }
    Dmod_FileClose(audio);
    return result;
}
```

For capture, enable `receive=on`, open the node for reading and use
`Dmod_FileRead`. For full duplex, enable both directions and open with `r+`.
The codec and GPIO configuration still belong to their respective drivers.

## Core and port layout

`dmsai` will handle the `dmdrvi` DIF, INI configuration and byte/frame semantics.
`dmsai_port` handles resource ownership, clocks and SAI registers. The
port API uses only portable `dmsai_types.h` types. Its STM32 definitions are
placed in `src/port/stm32_common/`; `src/port/stm32f7/port.c` contains only
module lifecycle and a small hardware descriptor. Another
STM32 family selects the same common source in its `config.cmake`. A different
architecture can select its own common source without changing the public API.

The STM32F7 port supports master I2S and TDM, 16-bit PCM or 24/32-bit PCM in
32-bit slots, up to 16 slots, 256 frame bits and 96 kHz. It asks `dmclk_port`
v1.2 for the SAI kernel clock before enabling the peripheral gate, then
releases the clock after stopping and gating SAI. With `MCKDIV=2`, a 48 kHz
stream requests 49.152 MHz and reports the achieved sample rate. SAI A
generates the clocks and transmits; SAI B receives synchronously when
requested. Board GPIO and codec setup remain separate.

If a synchronous block does not acknowledge a stop within five milliseconds,
the port resets that SAI controller, restores its configuration and increments
the transfer error counter. This also allows a later start or deinitialization
when an external frame signal is absent.

Read and write currently poll the SAI FIFOs. This supports short direct port
transfers and hardware bring-up; it does not provide continuous buffered audio
across scheduling gaps. A later DMA-backed transfer engine is needed for
sustained playback and recording. This limitation does not affect the clock,
configuration, start/stop and status operations.

`dmsai_port_board_test` is a direct hardware test on STM32F746G-DISCO. It
configures SAI2 at 48 kHz and 44.1 kHz, checks registers, starts both blocks,
writes one PCM frame, then checks that the clock and peripheral gate are
released. Run it from the firmware shell after bundling `dmsai_port`, the test
module and published `dmclk_port` v1.2. It does not initialize the WM8994
codec or verify sound on an external pin.

## Build and tests

```sh
cmake -S . -B build -DDMOD_CPU_FAMILY=stm32f7 -DDMOD_DIR=/path/to/dmod
cmake --build build
```

The existing `test_dmsai` target is retained. Running its Cortex-M7 module
requires a compatible loader or target board; the Raspberry Pi AArch64 loader
cannot execute it. See [API reference](docs/api-reference.md) for the detailed
contracts.
