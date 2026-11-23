#ifndef DMSAI_ESP32S3_I2S_H
#define DMSAI_ESP32S3_I2S_H

/*
 * Registers of the I2S controllers of the ESP32-S3 (ESP32-S3 Technical Reference Manual, I2S Controller) as the sequences of the
 * ESP-IDF HAL (hal/esp32s3/include/hal/i2s_ll.h) use them. Offsets and bit positions are from soc/esp32s3/include/soc/i2s_reg.h.
 */

#include <stdint.h>

#define I2S_BASE(i)             (0x6000F000UL + 0x1E000UL * (i))      /* I2S0, I2S1 */

#define I2S_RX_CONF             0x20U
#define I2S_TX_CONF             0x24U
#define I2S_RX_CONF1            0x28U
#define I2S_TX_CONF1            0x2CU
#define I2S_RX_CLKM_CONF        0x30U
#define I2S_TX_CLKM_CONF        0x34U
#define I2S_RX_CLKM_DIV_CONF    0x38U
#define I2S_TX_CLKM_DIV_CONF    0x3CU
#define I2S_RX_TDM_CTRL         0x50U
#define I2S_TX_TDM_CTRL         0x54U
#define I2S_RXEOF_NUM           0x64U
#define I2S_CONF_SINGLE_DATA    0x68U
#define I2S_STATE               0x6CU

/* RX_CONF / TX_CONF (the bits are the same unless noted) */
#define I2S_CONF_RESET          (1UL << 0)
#define I2S_CONF_FIFO_RESET     (1UL << 1)
#define I2S_CONF_START          (1UL << 2)
#define I2S_CONF_SLAVE_MOD      (1UL << 3)
#define I2S_CONF_MONO           (1UL << 5)
#define I2S_CONF_CHAN_EQUAL     (1UL << 6)      /* TX only */
#define I2S_CONF_BIG_ENDIAN     (1UL << 7)
#define I2S_CONF_UPDATE         (1UL << 8)
#define I2S_CONF_MONO_FST_VLD   (1UL << 9)
#define I2S_CONF_LEFT_ALIGN     (1UL << 15)
#define I2S_CONF_24_FILL_EN     (1UL << 16)
#define I2S_CONF_WS_IDLE_POL    (1UL << 17)
#define I2S_CONF_BIT_ORDER      (1UL << 18)     /* 1: LSB first */
#define I2S_CONF_TDM_EN         (1UL << 19)     /* the standard (Philips) mode is the TDM one with two channels */
#define I2S_CONF_PDM_EN         (1UL << 20)
#define I2S_RX_CONF_PDM2PCM_EN  (1UL << 21)
#define I2S_RX_CONF_PDM_DSR16   (1UL << 22)
#define I2S_TX_CONF_SIG_LOOPBACK (1UL << 27)    /* the RX shares the BCK and WS of the TX */

/* RX_CONF1 / TX_CONF1 */
#define I2S_CONF1_WS_WIDTH_S    0U              /* 7 bits: the width of WS in BCK cycles - 1 */
#define I2S_CONF1_BCK_DIV_S     7U              /* 6 bits: mclk / bclk - 1 */
#define I2S_CONF1_BITS_MOD_S    13U             /* 5 bits: the bits of a sample - 1 */
#define I2S_CONF1_HALF_BITS_S   18U             /* 6 bits: the bits of a half frame - 1 */
#define I2S_CONF1_CHAN_BITS_S   24U             /* 5 bits: the bits of a channel - 1 */
#define I2S_CONF1_MSB_SHIFT     (1UL << 29)     /* Philips: the data starts one BCK after the edge of WS */

/* RX_CLKM_CONF / TX_CLKM_CONF */
#define I2S_CLKM_DIV_NUM_S      0U              /* 8 bits: the integer part of the divider */
#define I2S_CLKM_CLK_ACTIVE     (1UL << 26)
#define I2S_CLKM_CLK_SEL_S      27U             /* 2 bits: 0 XTAL, 2 PLL_F160M, 3 external */
#define I2S_CLKM_CLK_SEL_PLL160 2UL
#define I2S_TX_CLKM_CLK_EN      (1UL << 29)     /* the clock of the whole controller */
#define I2S_RX_CLKM_MCLK_SEL    (1UL << 29)     /* 0: the MCLK is the TX one */

/* RX_CLKM_DIV_CONF / TX_CLKM_DIV_CONF */
#define I2S_DIV_Z_S             0U
#define I2S_DIV_Y_S             9U
#define I2S_DIV_X_S             18U
#define I2S_DIV_YN1_S           27U

/* RX_TDM_CTRL / TX_TDM_CTRL */
#define I2S_TDM_CHAN_EN_M       0xFFFFUL
#define I2S_TDM_TOT_CHAN_S      16U             /* 4 bits: the channels - 1 */
#define I2S_TX_TDM_SKIP_MSK_EN  (1UL << 20)

/* STATE */
#define I2S_STATE_TX_IDLE       (1UL << 0)

/* SYSTEM: the bus clock and the reset of the controllers (the same bits in PERIP_CLK_EN0 and PERIP_RST_EN0) */
#define SYSTEM_BASE             0x600C0000UL
#define SYSTEM_PERIP_CLK_EN0    (SYSTEM_BASE + 0x18UL)
#define SYSTEM_PERIP_RST_EN0    (SYSTEM_BASE + 0x20UL)
#define SYSTEM_I2S_BIT(i)       ((i) == 0 ? (1UL << 4) : (1UL << 21))

/* GPIO matrix and IO MUX (as in the UART port) */
#define GPIO_BASE                   0x60004000UL
#define GPIO_FUNC_IN_SEL_CFG(sig)   (GPIO_BASE + 0x154UL + 4UL * (sig))
#define GPIO_FUNC_OUT_SEL_CFG(pad)  (GPIO_BASE + 0x554UL + 4UL * (pad))
#define GPIO_IN_SEL_SIG_IN_SEL      (1UL << 7)      /* the signal is routed by the matrix */
#define IO_MUX_REG(pad)             (0x60009004UL + 4UL * (pad))
#define IO_MUX_FUN_IE               (1UL << 9)
#define IO_MUX_MCU_SEL_S            12U
#define IO_MUX_MCU_SEL_M            (7UL << IO_MUX_MCU_SEL_S)
#define IO_MUX_FUNC_GPIO            1UL
#define GPIO_PAD_COUNT              49U

/* Signals of the GPIO matrix (soc/gpio_sig_map.h); index 0 is I2S0, 1 is I2S1 */
typedef struct
{
    uint8_t mclk_out;
    uint8_t tx_bck_out, tx_ws_out, tx_data_out;
    uint8_t rx_bck_out, rx_ws_out, rx_data_in;     /* the master RX clocks, the data input */
} i2s_signals_t;

#define ESP32S3_I2S_COUNT       2U
#define GDMA_PERI_SEL_I2S(i)    (3U + (i))      /* the request of a peripheral transfer of dmdma */
#define I2S_SOURCE_CLOCK_HZ     160000000UL     /* PLL_F160M */
#define I2S_MCLK_MULTIPLE       256UL           /* the MCLK is 256 * the sample rate, the BCLK 64 * (a 32-bit slot) */

#endif /* DMSAI_ESP32S3_I2S_H */
