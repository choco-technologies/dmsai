# Module API

Include `dmsai.h`. Functions return zero on success, otherwise negative errno.
Context pointers are opaque and magic guarded. Successful create writes the
output pointer; failed create leaves it unchanged. Serialize thread operations
for each context. ISR callbacks must not call create/start/stop/destroy, allocate,
access files or program the codec; notify a worker instead.

| Operation | Semantics |
|---|---|
| `dmsai_create(config, &context)` | Validate format, reserve SAI2, DMA streams and the SAI clock |
| `dmsai_start(context, tx, rx, elements, callback, user)` | Start circular TX, and RX if enabled by config; callbacks run in IRQ context |
| `dmsai_stop(context)` | Stop blocks/DMA and detach callbacks; idempotent |
| `dmsai_get_status(context, &status)` | Consistent snapshot of cumulative counters, frequency and running flag |
| `dmsai_destroy(context)` | Stop and release resources; failure retains the context for retry |

Config: instance=2, rate=8000..96000, tolerance_ppm=0..10000, sample_bits=16/24/32,
slot_bits=16/32, slot_count=2/4/8/16 subject to frame<=256 bits, active_slots=nonzero
mask with an even population count. Master-clock generation requires power-of-two
frame lengths. Use a common active mask for A/B. On WM8994 the usual 16-bit frame
has four slots: mask `0x5` for headphones, `0xA` for speakers, `0xF` for both.
Codec configuration must match. The actual kernel frequency is returned in
status; actual_sample_rate is rounded kernel/256, not the requested rate.

`elements` counts active-slot samples, not bytes or frames. Width is 2 bytes for
16-bit samples, 4 bytes for 24/32-bit samples (sign/sample bits in the low bits).
Elements must be <=65535 and divisible by twice the number of active slots, so
each half contains complete frames. TX is mandatory; RX must be supplied exactly
when receive=true. Buffers must be aligned to sample width, disjoint and remain
valid until a successful stop/destroy. The STM32F746 port accepts DTCM buffers
within 0x20000000..0x2000ffff only. Refill the half reported by half/complete
notifications before DMA wraps to it; late refills can repeat stale samples.

Events: half and complete for each direction; DMA error; FIFO/frame error.
Hardware FIFO/frame faults are reported once per start to prevent IRQ storms.
An error does not automatically free buffers or stop the stream. Read counters,
then stop and handle recovery. Counters are cumulative across restarts.

Typical errors: EINVAL invalid config/buffers/context, ENOMEM allocation,
EBUSY resource/clock conflict or already running, ERANGE unachievable clock
within tolerance, ENOTSUP buffer outside supported DMA memory, ETIMEDOUT
hardware lock/stop timeout. dmclk failures are propagated unchanged.
