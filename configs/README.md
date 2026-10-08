# dmsai configuration files

`board/` contains GPIO routing and one `driver_name=dmsai` device section for
boards whose codec wiring is documented by ST. `mcu/` contains pin-agnostic
stereo defaults. Pick the board file **or** an MCU default for a controller;
loading both would try to create two nodes for the same SAI instance.

| File | Controller | Source of pin mapping |
|---|---|---|
| `board/stm32f746g-disco/sai2.ini` | SAI2, index 1 | [ST 32F746G BSP audio header](https://github.com/STMicroelectronics/32f746gdiscovery-bsp/blob/main/stm32746g_discovery_audio.h) |
| `board/stm32f769i-discovery/sai1.ini` | SAI1, index 0 | [ST STM32F769I BSP audio header](https://github.com/STMicroelectronics/32f769idiscovery-bsp/blob/main/stm32f769i_discovery_audio.h) |
| `mcu/stm32f746ng.ini` | SAI2, index 1 | No pins; generic stereo configuration |
| `mcu/stm32f769ni.ini` | SAI1, index 0 | No pins; generic stereo configuration |

`instance` is zero-based in this proposed API. The board configs use four
16-bit TDM slots with active mask 5 (slots 0 and 2 for WM8994 headphones).
The F746G BSP uses a 64-bit frame; the F769I output BSP uses a 128-bit frame,
so `frame_bits` is a separate setting from `slot_bits * slot_count`. The F769I
board config enables TX only because its BSP capture example uses a different
frame length. The generic MCU defaults use two 16-bit I2S slots and no board
pins.

The codec must be configured independently. These files are proposed inputs
for the future driver: the current `dmsai` stub returns `NULL` from
`dmdrvi_create()` and cannot expose a working `/dev/dmsaiN` device.
