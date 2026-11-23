set(DMOD_TOOLS_NAME "arch/xtensa/esp32s3" CACHE STRING "DMOD toolchain configuration")
# The I2S controller of the chip has its own registers (port.c); the DMA engine (a ring of two halves, the software queue, the
# format conversion) is the one of the STM32 SAI - the GDMA is reached through the same dmdma leases.
set(DMSAI_PORT_COMMON_SOURCES
    stm32_common/stm32_sai_dma.c)
# The clock of the I2S is the fixed 160 MHz PLL branch: nothing to ask dmclk for.
set(DMSAI_PORT_LINK_MODULES dmdma@>=0.8 dmheap dmosi)
