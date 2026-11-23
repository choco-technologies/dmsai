#define DMOD_ENABLE_REGISTRATION ON
#include "dmsai_port.h"
#include "esp32s3_i2s.h"
#include "../stm32_common/stm32_sai_dma.h"
#include "dmod.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

/*
 * The I2S port of the ESP32-S3: I2S0 and I2S1 (instance 0 and 1) in the standard (Philips) mode, the chip as the master of the clocks,
 * the TX and the RX in one controller (the RX takes the clocks of the TX when both are used).
 *
 * - Every slot is 32 bits on the wire (the frame is 64 BCK), a client's sample (16, 24 or 32 bits) is at the top of it, MSB first.
 *   A DAC like the PCM5102A or the PCM5101 does not care about the length of the slot.
 * - The serial clock comes from the PLL_F160M (160 MHz, it does not move when dmclk changes the CPU frequency), divided to
 *   256 * the sample rate by the fractional divider, then by 4 to the bit clock.
 * - The signals go to the pads through the GPIO matrix: the keys bclk_pin, ws_pin, dout_pin, din_pin and mclk_pin of the
 *   configuration (dmsai_config_t).
 * - The samples travel by the GDMA (dmdma leases; the engine is shared with the STM32 SAI port): the buffers are in the internal RAM.
 */

#define RX_HALF_FRAMES 32U   /* frames in half of a DMA buffer */
#define SLOT_BITS               32U
#define FRAME_BITS              (2U * SLOT_BITS)
#define BCK_DIV                 (I2S_MCLK_MULTIPLE / FRAME_BITS)    /* MCLK / BCLK */
#define RATE_MAX                96000U
#define STOP_WAIT_LOOPS         100000U

static const i2s_signals_t signals[ESP32S3_I2S_COUNT] = {
    { .mclk_out = 23, .tx_bck_out = 22, .tx_ws_out = 24, .tx_data_out = 25, .rx_bck_out = 26, .rx_ws_out = 27, .rx_data_in = 25 },
    { .mclk_out = 21, .tx_bck_out = 28, .tx_ws_out = 29, .tx_data_out = 30, .rx_bck_out = 31, .rx_ws_out = 32, .rx_data_in = 30 },
};

/* The GDMA streams of the controllers: from the top, dmspi takes the free ones from stream 0. The ring is two halves of 32 frames (512 bytes per
 * direction): the `dma` heap of the boards is small (32 KB) and the display takes most of it. */
static const stm32_sai_dma_route_t routes[ESP32S3_I2S_COUNT] = {
    { .controller = 0, .tx_stream = 4, .rx_stream = 3, .tx_request = GDMA_PERI_SEL_I2S(0), .rx_request = GDMA_PERI_SEL_I2S(0),
      .left_justify = true, .half_frames = RX_HALF_FRAMES },
    { .controller = 0, .tx_stream = 2, .rx_stream = 1, .tx_request = GDMA_PERI_SEL_I2S(1), .rx_request = GDMA_PERI_SEL_I2S(1),
      .left_justify = true, .half_frames = RX_HALF_FRAMES },
};

typedef struct
{
    dmsai_config_t config;
    dmsai_status_t status;
    uint8_t sample_bytes;
    uint8_t active_count;
    bool initialized;
    bool busy;
    bool tx_busy;
    bool rx_busy;
    stm32_sai_dma_t *dma;
} i2s_state_t;

static i2s_state_t states[ESP32S3_I2S_COUNT];

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *config)
{
    (void)config;
    return 0;
}

int dmod_deinit(void)
{
    for (unsigned i = 0; i < ESP32S3_I2S_COUNT; ++i)
        if (states[i].initialized || states[i].busy) return -EBUSY;
    return 0;
}

/* ---- registers ---- */

static inline volatile uint32_t *reg(uintptr_t address)
{
    return (volatile uint32_t *)address;
}

static inline volatile uint32_t *ireg(unsigned instance, uint32_t offset)
{
    return reg(I2S_BASE(instance) + offset);
}

/** Replace a field of a register: `width` bits at `shift` */
static void set_field(volatile uint32_t *r, unsigned shift, unsigned width, uint32_t value)
{
    uint32_t mask = ((1UL << width) - 1UL) << shift;
    *r = (*r & ~mask) | ((value << shift) & mask);
}

static i2s_state_t *get_state(dmsai_instance_t instance)
{
    return instance < ESP32S3_I2S_COUNT ? &states[instance] : NULL;
}

static uint8_t count_slots(uint32_t slots)
{
    return (uint8_t)__builtin_popcount(slots);
}

/* ---- validation ---- */

static int validate(const dmsai_config_t *config)
{
    if (!config || config->instance >= ESP32S3_I2S_COUNT || !config->sample_rate_hz || config->sample_rate_hz > RATE_MAX ||
        config->tolerance_ppm > 1000000UL || (unsigned)config->clock_role > dmsai_clock_slave ||
        (!config->transmit && !config->receive) || (unsigned)config->pcm_format > dmsai_pcm_s32_le)
        return -EINVAL;
    if (config->clock_role == dmsai_clock_slave) return -ENOTSUP;
    if (config->framing != dmsai_frame_i2s) return config->framing == dmsai_frame_tdm ? -ENOTSUP : -EINVAL;
    if (config->slot_count != 2 || config->active_slots != 3U) return -ENOTSUP;     /* stereo: both slots */
    if (config->slot_bits != SLOT_BITS || config->frame_bits != FRAME_BITS) return -ENOTSUP;
    if (config->transmit && (config->dout_pin < 0 || config->bclk_pin < 0 || config->ws_pin < 0)) return -EINVAL;
    if (!config->transmit && config->receive && (config->din_pin < 0 || config->bclk_pin < 0 || config->ws_pin < 0)) return -EINVAL;
    if (config->transmit && config->receive && config->din_pin < 0) return -EINVAL;
    const int8_t pins[] = { config->bclk_pin, config->ws_pin, config->dout_pin, config->din_pin, config->mclk_pin };
    for (unsigned i = 0; i < sizeof(pins); ++i)
        if (pins[i] >= (int)GPIO_PAD_COUNT) return -EINVAL;
    return 0;
}

/* ---- clock ---- */

/** The divider 160 MHz / (256 * rate) = integer + b/a, the coefficients as the ESP-IDF HAL calculates them. The result is the actual MCLK. */
static uint32_t calc_divider(uint32_t mclk, uint32_t *integer, uint32_t *denominator, uint32_t *numerator)
{
    uint32_t denom = 2, numer = 0;
    uint32_t integ = (uint32_t)(I2S_SOURCE_CLOCK_HZ / mclk);
    uint32_t error = (uint32_t)(I2S_SOURCE_CLOCK_HZ % mclk);
    const uint32_t max_fract = 512;
    if (error)
    {
        if (error < mclk - mclk / (max_fract - 1) * 2)
        {
            uint64_t best = UINT64_MAX;
            for (uint32_t a = 2; best && a < max_fract; ++a)
            {
                uint32_t b = (a * error + mclk / 2) / mclk;
                uint64_t lhs = (uint64_t)mclk * b, rhs = (uint64_t)error * a;
                uint64_t sub = lhs > rhs ? lhs - rhs : rhs - lhs;
                if (sub < best) { denom = a; numer = b; best = sub; }
            }
        }
        else integ++;
    }
    if (integ < 1 || integ >= 256) return 0;
    *integer = integ;
    *denominator = denom;
    *numerator = numer;
    if (numer)
    {
        uint32_t temp = integ * denom + numer;
        return (uint32_t)(((uint64_t)I2S_SOURCE_CLOCK_HZ * denom + temp / 2) / temp);
    }
    return (uint32_t)(I2S_SOURCE_CLOCK_HZ / integ);
}

/** The dividers of one direction (the TX or the RX registers): the MCLK and the BCLK of the sample rate. Returns the actual MCLK or 0. */
static uint32_t set_clock(unsigned instance, bool tx, uint32_t rate)
{
    uint32_t integer, denominator, numerator;
    uint32_t mclk = calc_divider(rate * (uint32_t)I2S_MCLK_MULTIPLE, &integer, &denominator, &numerator);
    if (!mclk) return 0;
    uint32_t x = 0, y = 0, z = 0, yn1 = 0;
    if (denominator && numerator)
    {
        yn1 = numerator * 2 > denominator;
        z = yn1 ? denominator - numerator : numerator;
        x = denominator / z - 1;
        y = denominator % z;
    }
    volatile uint32_t *clkm = ireg(instance, tx ? I2S_TX_CLKM_CONF : I2S_RX_CLKM_CONF);
    volatile uint32_t *div = ireg(instance, tx ? I2S_TX_CLKM_DIV_CONF : I2S_RX_CLKM_DIV_CONF);
    /* The HAL first goes through a safe divider: a switch from a low rate to a high one can leave the clock inaccurate */
    *div = (3UL << I2S_DIV_Z_S) | (7UL << I2S_DIV_Y_S) | (317UL << I2S_DIV_X_S);
    set_field(clkm, I2S_CLKM_DIV_NUM_S, 8, 7);
    set_field(clkm, I2S_CLKM_CLK_SEL_S, 2, I2S_CLKM_CLK_SEL_PLL160);
    *clkm |= I2S_CLKM_CLK_ACTIVE;
    set_field(clkm, I2S_CLKM_DIV_NUM_S, 8, integer);
    *div = (z << I2S_DIV_Z_S) | (y << I2S_DIV_Y_S) | (x << I2S_DIV_X_S) | (yn1 << I2S_DIV_YN1_S);
    set_field(ireg(instance, tx ? I2S_TX_CONF1 : I2S_RX_CONF1), I2S_CONF1_BCK_DIV_S, 6, BCK_DIV - 1U);
    return mclk;
}

/* ---- the pads ---- */

static bool pad_valid(int pad)
{
    return (pad >= 0 && pad <= 21) || (pad >= 26 && pad < (int)GPIO_PAD_COUNT);
}

/** A pad as a GPIO of the matrix, the input enabled (the level of an output can be read back, an input needs it) */
static void pad_to_matrix(int pad)
{
    volatile uint32_t *r = reg(IO_MUX_REG((uint32_t)pad));
    *r = (*r & ~IO_MUX_MCU_SEL_M) | (IO_MUX_FUNC_GPIO << IO_MUX_MCU_SEL_S) | IO_MUX_FUN_IE;
}

static void route_output(int pad, uint32_t signal)
{
    pad_to_matrix(pad);
    *reg(GPIO_FUNC_OUT_SEL_CFG((uint32_t)pad)) = signal;        /* the output enable is the signal's own */
}

static void route_input(int pad, uint32_t signal)
{
    pad_to_matrix(pad);
    *reg(GPIO_FUNC_IN_SEL_CFG(signal)) = GPIO_IN_SEL_SIG_IN_SEL | (uint32_t)pad;
}

static void release_output(int pad)
{
    if (pad >= 0) *reg(GPIO_FUNC_OUT_SEL_CFG((uint32_t)pad)) = 0x100UL;     /* a plain GPIO again (SIG_GPIO_OUT_IDX) */
}

static int route_pins(const dmsai_config_t *config)
{
    const i2s_signals_t *s = &signals[config->instance];
    const int8_t pins[] = { config->bclk_pin, config->ws_pin, config->dout_pin, config->din_pin, config->mclk_pin };
    for (unsigned i = 0; i < sizeof(pins); ++i)
        if (pins[i] >= 0 && !pad_valid(pins[i])) return -EINVAL;
    /* The master drives the clocks from the TX side when it transmits, from the RX side when it only receives */
    if (config->bclk_pin >= 0) route_output(config->bclk_pin, config->transmit ? s->tx_bck_out : s->rx_bck_out);
    if (config->ws_pin >= 0) route_output(config->ws_pin, config->transmit ? s->tx_ws_out : s->rx_ws_out);
    if (config->transmit && config->dout_pin >= 0) route_output(config->dout_pin, s->tx_data_out);
    if (config->receive && config->din_pin >= 0) route_input(config->din_pin, s->rx_data_in);
    if (config->mclk_pin >= 0) route_output(config->mclk_pin, s->mclk_out);
    return 0;
}

static void release_pins(const dmsai_config_t *config)
{
    release_output(config->bclk_pin);
    release_output(config->ws_pin);
    if (config->transmit) release_output(config->dout_pin);
    release_output(config->mclk_pin);
}

/* ---- the controller ---- */

/** The standard (Philips) mode of one direction: 32-bit slots, MSB first, the data one BCK after the edge of WS, stereo */
static void set_slot(unsigned instance, bool tx, bool slave)
{
    volatile uint32_t *conf = ireg(instance, tx ? I2S_TX_CONF : I2S_RX_CONF);
    volatile uint32_t *conf1 = ireg(instance, tx ? I2S_TX_CONF1 : I2S_RX_CONF1);
    volatile uint32_t *tdm = ireg(instance, tx ? I2S_TX_TDM_CTRL : I2S_RX_TDM_CTRL);
    *conf |= I2S_CONF_RESET;
    *conf &= ~I2S_CONF_RESET;
    *conf &= ~(I2S_CONF_SLAVE_MOD | I2S_CONF_MONO | I2S_CONF_MONO_FST_VLD | I2S_CONF_BIG_ENDIAN | I2S_CONF_24_FILL_EN |
               I2S_CONF_WS_IDLE_POL | I2S_CONF_BIT_ORDER | I2S_CONF_PDM_EN | (tx ? I2S_CONF_CHAN_EQUAL : I2S_RX_CONF_PDM2PCM_EN));
    *conf |= I2S_CONF_TDM_EN | I2S_CONF_LEFT_ALIGN | (slave ? I2S_CONF_SLAVE_MOD : 0UL);
    uint32_t v = *conf1 & (0x3FUL << I2S_CONF1_BCK_DIV_S);              /* the bit clock divider is set with the clock */
    v |= ((SLOT_BITS - 1U) << I2S_CONF1_WS_WIDTH_S) | ((SLOT_BITS - 1U) << I2S_CONF1_BITS_MOD_S) |
         ((SLOT_BITS - 1U) << I2S_CONF1_HALF_BITS_S) | ((SLOT_BITS - 1U) << I2S_CONF1_CHAN_BITS_S) | I2S_CONF1_MSB_SHIFT;
    *conf1 = v;
    *tdm = (1UL << I2S_TDM_TOT_CHAN_S) | 3UL;                           /* two channels, both enabled, none skipped */
}

static void update_and_start(unsigned instance, bool tx)
{
    volatile uint32_t *conf = ireg(instance, tx ? I2S_TX_CONF : I2S_RX_CONF);
    *conf |= I2S_CONF_UPDATE;
    uint32_t guard = STOP_WAIT_LOOPS;
    while ((*conf & I2S_CONF_UPDATE) && --guard) { }
    *conf |= I2S_CONF_START;
}

static void stop_direction(unsigned instance, bool tx)
{
    *ireg(instance, tx ? I2S_TX_CONF : I2S_RX_CONF) &= ~I2S_CONF_START;
}

static void reset_fifo(unsigned instance, bool tx)
{
    volatile uint32_t *conf = ireg(instance, tx ? I2S_TX_CONF : I2S_RX_CONF);
    *conf |= I2S_CONF_RESET | I2S_CONF_FIFO_RESET;
    *conf &= ~(I2S_CONF_RESET | I2S_CONF_FIFO_RESET);
}

/** Program the whole controller. With both directions the RX is a slave of the TX clocks (they share BCK and WS inside the chip). */
static int configure(unsigned instance, const dmsai_config_t *config, uint32_t *actual_mclk)
{
    bool duplex = config->transmit && config->receive;
    *reg(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_I2S_BIT(instance);
    *reg(SYSTEM_PERIP_RST_EN0) |= SYSTEM_I2S_BIT(instance);
    *reg(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_I2S_BIT(instance);
    *ireg(instance, I2S_TX_CLKM_CONF) |= I2S_TX_CLKM_CLK_EN;
    uint32_t mclk = 0;
    if (config->transmit)
    {
        set_slot(instance, true, false);
        mclk = set_clock(instance, true, config->sample_rate_hz);
        if (!mclk) return -ENOTSUP;
        *ireg(instance, I2S_RX_CLKM_CONF) &= ~I2S_RX_CLKM_MCLK_SEL;     /* the MCLK pad follows the TX divider */
    }
    if (config->receive)
    {
        /* The receive DMA ends a descriptor after this many bytes (default 64): one half buffer */
        *ireg(instance, I2S_RXEOF_NUM) = RX_HALF_FRAMES * 2U * sizeof(uint32_t);
        set_slot(instance, false, duplex);
        uint32_t rx_mclk = set_clock(instance, false, config->sample_rate_hz);
        if (!rx_mclk) return -ENOTSUP;
        if (!config->transmit)
        {
            mclk = rx_mclk;
            *ireg(instance, I2S_RX_CLKM_CONF) |= I2S_RX_CLKM_MCLK_SEL;
        }
    }
    volatile uint32_t *tx_conf = ireg(instance, I2S_TX_CONF);
    if (duplex) *tx_conf |= I2S_TX_CONF_SIG_LOOPBACK;
    else *tx_conf &= ~I2S_TX_CONF_SIG_LOOPBACK;
    *actual_mclk = mclk;
    return 0;
}

static void power_down(unsigned instance)
{
    *ireg(instance, I2S_TX_CLKM_CONF) &= ~(I2S_TX_CLKM_CLK_EN | I2S_CLKM_CLK_ACTIVE);
    *ireg(instance, I2S_RX_CLKM_CONF) &= ~I2S_CLKM_CLK_ACTIVE;
    *reg(SYSTEM_PERIP_CLK_EN0) &= ~SYSTEM_I2S_BIT(instance);
}

/* ---- the port API ---- */

dmod_dmsai_port_api_declaration(1.0, int, _get_instance_count, ( void ))
{
    return ESP32S3_I2S_COUNT;
}

dmod_dmsai_port_api_declaration(1.0, int, _init, ( const dmsai_config_t *config ))
{
    int rc = validate(config);
    if (rc != 0) return rc;
    unsigned index = config->instance;
    i2s_state_t *state = &states[index];
    if (state->initialized || state->busy) return -EBUSY;
    state->busy = true;
    uint32_t mclk = 0;
    rc = configure(index, config, &mclk);
    if (rc == 0)
    {
        uint32_t actual = (uint32_t)((mclk + I2S_MCLK_MULTIPLE / 2) / I2S_MCLK_MULTIPLE);
        uint32_t error = actual > config->sample_rate_hz ? actual - config->sample_rate_hz : config->sample_rate_hz - actual;
        if ((uint64_t)error * 1000000ULL > (uint64_t)config->sample_rate_hz * config->tolerance_ppm)
            rc = -ERANGE;
        else
        {
            memset(state, 0, sizeof(*state));
            state->busy = true;
            state->config = *config;
            state->sample_bytes = config->pcm_format == dmsai_pcm_s16_le ? 2U : 4U;
            state->active_count = count_slots(config->active_slots);
            state->status.actual_sample_rate_hz = actual;
            /* The GDMA addresses the FIFO of the I2S by the PERI_SEL of the route and ignores the address of the peripheral side; dmdma still wants a
             * non-null aligned one */
            uintptr_t dr = I2S_BASE(index) + I2S_CONF_SINGLE_DATA;
            rc = stm32_sai_dma_create(&state->dma, &routes[index], dr, dr, config, &state->status);
        }
    }
    if (rc == 0) rc = route_pins(config);
    if (rc == 0) state->initialized = true;
    else
    {
        if (state->dma) { stm32_sai_dma_destroy(state->dma); state->dma = NULL; }
        power_down(index);
    }
    state->busy = false;
    return rc;
}

dmod_dmsai_port_api_declaration(1.0, int, _stop, ( dmsai_instance_t instance ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    __atomic_store_n(&state->status.running, false, __ATOMIC_RELEASE);
    if (state->config.transmit) stop_direction(instance, true);
    if (state->config.receive) stop_direction(instance, false);
    stm32_sai_dma_stop(state->dma);
    if (state->config.transmit) reset_fifo(instance, true);
    if (state->config.receive) reset_fifo(instance, false);
    return 0;
}

dmod_dmsai_port_api_declaration(1.0, int, _deinit, ( dmsai_instance_t instance ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    if (state->busy || state->tx_busy || state->rx_busy) return -EBUSY;
    int rc = dmsai_port_stop(instance);
    if (rc != 0) return rc;
    stm32_sai_dma_destroy(state->dma);
    release_pins(&state->config);
    power_down(instance);
    memset(state, 0, sizeof(*state));
    return 0;
}

dmod_dmsai_port_api_declaration(1.0, int, _start, ( dmsai_instance_t instance ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    if (__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return 0;
    if (state->config.transmit) reset_fifo(instance, true);
    if (state->config.receive) reset_fifo(instance, false);
    int rc = stm32_sai_dma_start(state->dma);
    if (rc != 0) return rc;
    __atomic_store_n(&state->status.running, true, __ATOMIC_RELEASE);
    /* The receiver first: it is a slave of the transmitter's clocks in the duplex mode, which start with the transmitter */
    if (state->config.receive) update_and_start(instance, false);
    if (state->config.transmit) update_and_start(instance, true);
    return 0;
}

dmod_dmsai_port_api_declaration(1.0, int, _read,
    ( dmsai_instance_t instance, void *buffer, size_t size, size_t *received, uint32_t timeout_ms ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized || !state->config.receive) return -ENODEV;
    size_t frame_bytes = state->active_count * state->sample_bytes;
    if (!received || (!buffer && size) || size % frame_bytes) return -EINVAL;
    *received = 0;
    if (!__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return -EPIPE;
    if (__atomic_exchange_n(&state->rx_busy, true, __ATOMIC_ACQ_REL)) return -EBUSY;
    int rc = stm32_sai_dma_read(state->dma, buffer, size, received, timeout_ms);
    __atomic_store_n(&state->rx_busy, false, __ATOMIC_RELEASE);
    return rc;
}

dmod_dmsai_port_api_declaration(1.0, int, _write,
    ( dmsai_instance_t instance, const void *buffer, size_t size, size_t *written, uint32_t timeout_ms ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized || !state->config.transmit) return -ENODEV;
    size_t frame_bytes = state->active_count * state->sample_bytes;
    if (!written || (!buffer && size) || size % frame_bytes) return -EINVAL;
    *written = 0;
    if (!__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return -EPIPE;
    if (__atomic_exchange_n(&state->tx_busy, true, __ATOMIC_ACQ_REL)) return -EBUSY;
    int rc = stm32_sai_dma_write(state->dma, buffer, size, written, timeout_ms);
    __atomic_store_n(&state->tx_busy, false, __ATOMIC_RELEASE);
    return rc;
}

dmod_dmsai_port_api_declaration(1.0, int, _flush, ( dmsai_instance_t instance, uint32_t timeout_ms ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized || !state->config.transmit) return -ENODEV;
    if (!__atomic_load_n(&state->status.running, __ATOMIC_ACQUIRE)) return -EPIPE;
    if (__atomic_exchange_n(&state->tx_busy, true, __ATOMIC_ACQ_REL)) return -EBUSY;
    int rc = stm32_sai_dma_flush(state->dma, timeout_ms);
    __atomic_store_n(&state->tx_busy, false, __ATOMIC_RELEASE);
    return rc;
}

dmod_dmsai_port_api_declaration(1.0, int, _get_status, ( dmsai_instance_t instance, dmsai_status_t *status ))
{
    i2s_state_t *state = get_state(instance);
    if (!state || !state->initialized) return -ENODEV;
    if (!status) return -EINVAL;
    *status = state->status;
    return 0;
}
