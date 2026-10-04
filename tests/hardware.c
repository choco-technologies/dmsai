/* STM32F746G-DISCO transport test, no codec driver or acoustic validation. */
#include "dmsai.h"
#include "dmheap.h"
#include <string.h>
#include <errno.h>
#define GPIO(base, offset) (*(volatile uint32_t *)((base) + (offset)))
#define GPIOI 0x40022000UL
#define GPIOG 0x40021800UL
static uint32_t saved_i_mode, saved_i_af, saved_g_mode, saved_g_af, saved_clocks;
static void pins(bool enable)
{
    volatile uint32_t *rcc = (uint32_t *)0x40023830UL;
    if (enable)
    {
        saved_clocks = *rcc; *rcc |= (1UL << 8) | (1UL << 6); (void)*rcc;
        saved_i_mode = GPIO(GPIOI, 0); saved_i_af = GPIO(GPIOI, 0x20);
        saved_g_mode = GPIO(GPIOG, 0); saved_g_af = GPIO(GPIOG, 0x24);
        GPIO(GPIOI, 0) = (saved_i_mode & ~0xFF00UL) | 0xAA00UL;
        GPIO(GPIOI, 0x20) = (saved_i_af & ~0xFFFF0000UL) | 0xAAAA0000UL;
        GPIO(GPIOG, 0) = (saved_g_mode & ~(3UL << 20)) | (2UL << 20);
        GPIO(GPIOG, 0x24) = (saved_g_af & ~(15UL << 8)) | (10UL << 8);
    }
    else
    {
        GPIO(GPIOI, 0) = saved_i_mode; GPIO(GPIOI, 0x20) = saved_i_af;
        GPIO(GPIOG, 0) = saved_g_mode; GPIO(GPIOG, 0x24) = saved_g_af;
        *rcc = (*rcc & ~((1UL << 8) | (1UL << 6))) | (saved_clocks & ((1UL << 8) | (1UL << 6)));
    }
}
static int run_rate(uint32_t rate, uint8_t bits, bool receive)
{
    dmsai_config_t cfg = {2, rate, 500, bits, bits == 16 ? 16 : 32, 4, 5, receive};
    dmsai_context_t context = NULL, duplicate = NULL;
    dmheap_context_t *heap = dmheap_get_context_by_name("dma");
    if (!heap) return -ENODEV;
    size_t width = bits == 16 ? 2 : 4, elements = 960;
    void *tx = dmheap_aligned_alloc(heap, 32, elements * width, "dmsai_hardware_test");
    void *rx = receive ? dmheap_aligned_alloc(heap, 32, elements * width, "dmsai_hardware_test") : NULL;
    int result = (!tx || (receive && !rx)) ? -ENOMEM : dmsai_create(&cfg, &context);
    if (tx) memset(tx, 0, elements * width);
    if (rx) memset(rx, 0xA5, elements * width);
    if (!result && dmsai_create(&cfg, &duplicate) != -EBUSY) result = -EIO;
    if (duplicate) dmsai_destroy(duplicate);
    if (!result) result = dmsai_start(context, tx, rx, elements, NULL, NULL);
    if (!result)
    {
        Dmod_ThreadSleep(1000);
        dmsai_status_t s = {0}; result = dmsai_get_status(context, &s);
        Dmod_Printf("DMSAI rate=%u bits=%u rx=%u kernel=%u actual=%u half=%u/%u full=%u/%u dma=%u/%u fifo=%u/%u\n",
                    rate, bits, receive, s.kernel_frequency, s.actual_sample_rate,
                    s.half_events[0], s.half_events[1], s.complete_events[0], s.complete_events[1],
                    s.dma_errors[0], s.dma_errors[1], s.fifo_errors[0], s.fifo_errors[1]);
        unsigned expected = rate / 480;
        if (s.complete_events[0] < expected - 5 || s.complete_events[0] > expected + 5 ||
            s.dma_errors[0] || s.fifo_errors[0] ||
            (receive && (s.complete_events[1] < expected - 5 || s.complete_events[1] > expected + 5 ||
                         s.dma_errors[1] || s.fifo_errors[1]))) result = -EIO;
        int stopped = dmsai_stop(context);
        if (!result) result = stopped;
        if (!result) /* restarting must preserve the context/clock leases */
        {
            result = dmsai_start(context, tx, rx, elements, NULL, NULL);
            Dmod_ThreadSleep(20);
            stopped = dmsai_stop(context); if (!result) result = stopped;
        }
    }
    if (context)
    {
        int released = dmsai_destroy(context);
        if (released) return released; /* Keep buffers alive if DMA cannot stop. */
    }
    if (tx) dmheap_free(heap, tx, true);
    if (rx) dmheap_free(heap, rx, true);
    return result;
}
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    pins(true);
    int result = run_rate(48000, 16, false);
    if (!result) result = run_rate(44100, 16, true);
    if (!result) result = run_rate(48000, 24, true);
    if (!result) result = run_rate(48000, 32, true);
    pins(false);
    Dmod_Printf("DMSAI_HARDWARE %s result=%d\n", result ? "FAIL" : "PASS", result);
    return result;
}
