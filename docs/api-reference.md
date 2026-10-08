# dmsai API reference

`include/dmsai.h`, `include/dmsai_types.h` and `include/dmsai_port.h` contain
the Doxygen contract for every entry point and type. All public entry points
currently return `-ENOSYS` without modifying output arguments. The API is a
proposal; the examples in README document intended future behavior.

## Configuration

`dmsai_config_t` has no register addresses, DMA channel numbers or STM32 clock
identifiers. `instance` is a zero-based target peripheral index; its mapping
belongs to the selected port. `format` selects I2S or TDM frame semantics.
`role` selects whether the endpoint supplies or receives clocks. At least one
of `transmit` and `receive` must be enabled. `active_slots` selects slots in
the range `0..slot_count-1`; for standard stereo I2S use `slot_count=2` and
`active_slots=0x3`. A future port validates supported widths, rates, masks,
clock tolerance and direction combinations.

## Transfer

`dmsai_start(context, tx, rx, elements, callback, user)` borrows the buffers.
Each non-NULL buffer holds `elements` samples packed in ascending active-slot
order. `elements` must be divisible by `2 * popcount(active_slots)`, which
makes each buffer half contain complete frames. A 16-bit sample uses a
`uint16_t` element; samples wider than 16 bits use `uint32_t`. Memory alignment
and DMA visibility are port requirements. The two buffers cannot overlap.

On `dmsai_event_half`, the first half is available. On
`dmsai_event_complete`, the second half is available. `dmsai_event_dma_error`
and `dmsai_event_frame_error` signal faults; a caller should stop the stream
and perform recovery outside interrupt context. The callback runs in interrupt
context and must not call lifecycle functions.

`dmsai_get_status` returns cumulative event counters indexed by
`dmsai_direction_t`, the actual sample rate and the running flag. The core
will own the counters so that ports only report events.

## Port contract

The four `dmsai_port_*` functions mirror the hardware-sensitive operations.
`dmsai_port_create` reserves resources and reports the actual frame rate;
`start` and `stop` manage DMA and interrupt callbacks; `destroy` releases
resources. All ports receive the same `dmsai_config_t`. A port does not parse
application configuration or expose peripheral register types through headers.

The STM32F7 layout demonstrates the planned sharing boundary:

```text
src/port/stm32_common/common.c  shared STM32 port entry points and future logic
src/port/stm32f7/port.c         family lifecycle and future IRQ routing
src/port/stm32f7/config.cmake   toolchain and shared-source selection
```

Another STM32 family can select the same common source and supply its small
family files. A non-STM32 port selects its own implementation source.
