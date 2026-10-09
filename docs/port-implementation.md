# Adding a New MCU Port to dmsai

`dmsai_port` currently supports `stm32f7`. The port is split from
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
