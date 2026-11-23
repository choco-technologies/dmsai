# Adding a New MCU Port to dmsai

`dmsai_port` currently supports `stm32f7` and `esp32s3`. The port is split from
the core `dmsai` module so a new architecture can be added without
touching architecture-independent logic.

## Steps to add another architecture

1. Create `src/port/<family>/config.cmake`, setting `DMOD_TOOLS_NAME` for the
   target architecture (see `src/port/stm32f7/config.cmake` for the pattern -
   it must match a directory under `dmod/configs/arch/...`).
2. Create `src/port/<family>/port.c` for `dmod_init`/`dmod_deinit` and any
   `DMOD_IRQ_HANDLER(...)` needed. Implement the functions declared in
   `include/dmsai_port.h` in a shared source file when the peripheral IP is
   common to multiple families. For STM32, these entry points are in
   `src/port/stm32_common/stm32_common.c` and selected by `config.cmake` through
   `DMSAI_PORT_COMMON_SOURCES`. The same common source also owns the DMA
   ping-pong engine in `stm32_sai_dma.c`; a family descriptor supplies only
   its SAI register addresses, clock domains and DMA stream/request routes.
3. Keep `port.c` limited to family-specific lifecycle and interrupt routing.
   A family with different peripheral IP may provide its own implementations
   of the port API through `DMSAI_PORT_COMMON_SOURCES`.
4. Build by selecting the new family:
   `cmake .. -DDMOD_CPU_FAMILY=<family>`.
5. Do not introduce a module-specific variable (e.g. `<MODULE>_MCU_SERIES`) for
   this - `DMOD_CPU_FAMILY` is the ecosystem-wide convention, already wired
   into `dmf-get` package resolution.

The board must configure SAI pins before `dmsai_port_init`. A direct port test
must route the target board's pins and restore their state afterward. The port
uses `dmdma` leases and obtains circular buffers from the uncached `dma` heap;
new families must provide an accessible DMA heap and valid routes.

## ESP32-S3 (I2S)

`src/port/esp32s3/port.c` implements the port API on the I2S controllers of the chip (I2S0, I2S1 = instance 0, 1) and shares the DMA engine
of the STM32 port (`stm32_common/stm32_sai_dma.c`; the GDMA is reached through the same dmdma leases, the route's request is the PERI_SEL).

* Standard (Philips) mode only, the chip is the master. Every slot is 32 bits on the wire (`slot_bits=32`, `frame_bits=64`, `slot_count=2`,
  `active_slots=3`); the PCM format of the client may be s16, s24 or s32 - the sample sits at the top of the slot (`left_justify` of the route).
* The bit clock is derived from the PLL_F160M: MCLK = 256 * rate by the fractional divider, BCLK = MCLK / 4. `tolerance_ppm` bounds the
  error of the divider.
* Pads: the keys `bclk_pin`, `ws_pin`, `dout_pin`, `din_pin`, `mclk_pin` route the signals through the GPIO matrix (no dmgpio sections).
  `din_pin` = `dout_pin` loops the node back on itself.
* With both directions the RX is a slave of the TX clocks (they share BCK and WS inside the chip).
* GDMA streams: I2S0 TX 4 / RX 3, I2S1 TX 2 / RX 1 (dmspi takes the free ones from stream 0). Needs the dmdma with the ESP32-S3 port.
* Not implemented: PDM RX (the microphone of the T-Deck Pro), TDM, slave role, 16-bit slots.
