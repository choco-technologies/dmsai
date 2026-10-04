# STM32F7 port

Initial supported configuration: STM32F746 SAI2 block A master TX, block B
optional synchronous slave RX. Other peripheral roles/instances are outside
this initial port. One context exclusively owns the SAI2 pair.

Uses dmclk's PLLI2S reservation at `sample_rate * 256` Hz; MCKDIV=0 supplies
MCLK=256*FS. PLLM, SYSCLK and LCD PLLSAI remain unchanged. Releases the clock
only after SAI is stopped and gated. The clock/DMA leases persist over stop
and are released on destroy. A foreign enabled SAI gate is never reset.

DMA is circular, with half/complete IRQ notifications and direct-mode sample
width. DMA vectors belong exclusively to dmdma; this port registers SAI2
IRQ91 for FIFO/frame faults. Shutdown disables the SAI IRQ, stops B before
A while its synchronization clock still exists, stops DMA, clears callbacks,
and frees the context only after success. Failures retain ownership for retry.

TX/RX memory is restricted to the non-cacheable 64 KiB DTCM heap on STM32F746,
which DMA2 can access through AHBS. Cached SRAM/SDRAM and other CPU families
need additional memory/cache contracts, not a silent fall-through.

Register/connection references: ST RM0385, ST cmsis-device-f7/stm32f746xx.h,
and ST 32f746gdiscovery-bsp/stm32746g_discovery_audio.[ch].
