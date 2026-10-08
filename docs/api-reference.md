# dmsai device API

The public device contract is the `dmdrvi` 2.0 DIF from `dmdrvi.h`. `dmdevfs`
finds `driver_name=dmsai`, passes the selected `dmini_context_t` to
`dmdrvi_create` and exposes the resulting major-numbered device node. The
driver implements the standard DIF methods; applications reach it through
the DMOD file API, not a `dmsai_create` function.

All DIF functions are currently stubs. `create` and `open` return `NULL`;
integer operations return `-ENOSYS`; `free` and `close` do nothing. This means
the current revision registers an interface but cannot expose a working node.

## Configuration and node

`include/dmsai_types.h` describes the portable configuration. `instance` is
zero-based, matching the intended `/dev/dmsaiN` major number. The core will
read the selected INI section's `clock_role`, `framing`, `pcm_format`,
`sample_rate_hz`, `tolerance_ppm`, `slot_bits`, `slot_count`, `frame_bits`, `active_slots`,
`transmit` and `receive` keys. At least one direction must be enabled.
`active_slots` is a mask within the configured slot count. Standard stereo I2S
uses `slot_count=2`, `frame_bits=32`, `active_slots=3` with 16-bit slots.
`frame_bits` includes any padding after the selected slots. The port decides which combinations are
available on a given target.

## PCM I/O

`dmdrvi_read` and `dmdrvi_write` use byte counts and return byte counts or
negative errno values. The frame size is `popcount(active_slots)` multiplied
by the PCM sample storage width (2 bytes for `s16_le`, 4 bytes otherwise).
The request size and successful byte count are whole-frame multiples. The
data is interleaved in ascending active-slot order. Offsets have no meaning
for a stream; a future implementation will reject negative offsets and ignore
non-negative offsets. A zero-length request returns zero. Read/write access
requires the corresponding direction to be enabled and a successful START.

`DMSAI_IOCTL_START` and `DMSAI_IOCTL_STOP` control the stream. GET_CONFIG and
GET_STATUS return the effective configuration and current counters. Timeout
commands set or get a per-open-handle millisecond bound for blocking I/O;
zero means wait indefinitely. All commands occupy the driver-specific range
beginning at `DMDRVI_IOCTL_CUSTOM_BASE`. Unknown commands will return
`-ENOTTY`. The precise argument types are documented in `dmsai_ioctl.h`.
Opening a node is exclusive; a second open fails until the first handle closes.

`dmdrvi_flush` will wait for queued TX samples to reach the output. `stat`
will report a non-seekable stream with size zero. The current stubs implement
none of this behavior.

## Port boundary

`include/dmsai_port.h` declares eight functions: instance count,
init/deinit, start/stop, read/write and status. The port receives the same
portable `dmsai_config_t` as the core and owns its hardware resources.
`read`/`write` copy whole frames between those buffers and caller memory.
It does not know about `dmdevfs`, INI files, file handles or `dmdrvi` types.

STM32 port API entry points reside in `src/port/stm32_common/stm32_common.c`; the
family `port.c` is reserved for lifecycle, hardware descriptors and IRQ
routing. Future STM32 families can share the common source. The STM32F7 port
uses `dmclk_port` v1.2 to reserve SAI clocks, configures master block A and
optional synchronous RX block B, and polls their FIFOs for direct transfers.
It accepts frames of at most 256 bits. If a block cannot complete a stop at a
frame boundary, the port resets and reconfigures that controller and records a
transfer error.
The architecture-independent `dmdrvi` core remains a stub.
