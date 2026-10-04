# STM32F746G-DISCO configuration

Load GPIO routing before starting SAI. Existing dmclk must initialize the main
clock first; both dmdma controllers can be created using their board INI.
The supplied `pins.ini` only configures GPIO and does not start an audio device.

SAI2 A: PI4 MCLK, PI5 SCK, PI6 SD, PI7 FS, AF10.
SAI2 B: PG10 SD, AF10. RX is internally synchronous to block A.
DMA2: TX stream4/channel3; RX stream7/channel0. SDMMC1 uses stream3/6/channel4
in the existing dmsdio port, so those stream reservations are separate.

Configure WM8994 independently over I2C3 PH7/PH8, unshifted address 0x1a.
The codec must use compatible frame/slot/sample settings before meaningful
playback/recording. The digital microphones feed WM8994, not a separate STM32
PDM peripheral. No codec or microphone init is performed by this module.
