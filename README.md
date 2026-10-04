# dmsai

SAI transport driver for DMOD. `dmsai` provides an opaque-context Module API;
`dmsai_port` implements STM32F746 SAI2 A master TX and optional B synchronous
RX with circular DMA. This first port supports 16/24/32-bit samples and
configurable active slots. Codec control belongs in a separate driver.

## Dependencies and build

Requires dmdma >= 0.5 and **dmclk >= 1.2 with the SAI clock reservation API**.
Until dmclk 1.2 is released, build the companion dmclk PR and supply its packaged
headers explicitly. No dependency source is statically linked into dmsai.

```sh
cmake -S ../dmclk -B ../dmclk/build-sai -DDMOD_MODULE_VERSION=1.2
cmake --build ../dmclk/build-sai
cmake -S . -B build -DDMOD_CPU_FAMILY=stm32f7 \
  -DDMSAI_DMCLK_INCLUDE_DIR="$PWD/../dmclk/build-sai/packages/dmclk/include"
cmake --build build
```

The local header override does not install the runtime dependency. Bundle the
matching dmclk/dmclk_port builds in firmware along with dmdma/dmdma_port and
both dmsai modules. Generated `.dmd` files retain the dmclk >= 1.2 requirement.
For the SDK itself, normal FetchContent fetches dmod/develop; a local SDK can
be selected using `-DFETCHCONTENT_SOURCE_DIR_DMOD=/path/to/dmod`.

## Use

Configure the SAI pins first (GPIO-only INI supplied in `configs/board/`).
Call `dmsai_create`, then `dmsai_start` with buffers allocated from the named
`dmheap` **dma** heap. Refill/consume completed halves on DMA notifications.
Call `dmsai_stop` before releasing those buffers and `dmsai_destroy` afterwards.
An RX stream still requires a TX buffer: block A generates the shared clocks.
Buffers in cached SRAM/SDRAM are rejected by the initial port.

This is a transport Module API, with no `/dev/audio` interface yet. A higher
layer can combine it with a codec driver and expose PCM file operations.
It does not initialize WM8994, route analog audio, or select a microphone.

## Tests

```sh
cmake -S tests/native -B build-native
cmake --build build-native
ctest --test-dir build-native --output-on-failure
```

These tests exercise the production core against a fake port. They check
configuration/buffer validation, lifecycle, error propagation, events and
status. The ARM `dmsai_hardware_test` runs on STM32F746G-DISCO with initialized
clock and DMA drivers. It checks 44.1/48 kHz, 16/24/32 bits, TX and TX/RX DMA
progress, stop/restart and resource conflicts. It restores its GPIO settings.
It neither proves analog sound quality nor validates microphone samples.

## Project structure

- `include/`: public core/port contracts and PCM types
- `src/dmsai.c`: validation, opaque context, notifications/status
- `src/port/stm32f7/`: RCC/SAI registers, clock and DMA resource ownership, IRQ
- `configs/board/stm32f746g-disco/`: GPIO routing
- `tests/native/`: host contract tests
- `tests/hardware.c`: physical board transport test
- `docs/`: API, configuration and port documentation
- `manifest.dmm`, `*.dmr`: independently packaged core and port modules

MIT; author Patryk Kubiak.
