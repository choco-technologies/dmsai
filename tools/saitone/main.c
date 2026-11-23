#include "dmod.h"
#include "dmdrvi_ioctl.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

/* A sine through a dmsai node (or any audio node with the stream contract): saitone [NODE [FREQ_HZ [SECONDS [loop]]]]
 * With "loop" it also reads the node (a receiver in the same controller, its data pad wired or routed to the transmitter's) and reports
 * the peak of what it got and how many frames were silent. Exit status 0 when the stream ran without errors. */

#define CHUNK_FRAMES    256U
#define AMPLITUDE_PCT   25U     /* of the full scale: a line output, not a speaker */

static const int16_t sine64[64] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787,
    23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30273, 28898, 27245, 25329,
    23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609,
    -32767, -32609, -32137, -31356, -30273, -28898, -27245, -25329,
    -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212,
};

static unsigned parse(const char *s, unsigned fallback)
{
    if (!s || !*s) return fallback;
    unsigned v = 0;
    for (; *s; ++s)
    {
        if (*s < '0' || *s > '9') return fallback;
        v = v * 10U + (unsigned)(*s - '0');
    }
    return v;
}

/** A 16-bit sample in the format of the stream (the sample bytes at the little-endian position) */
static void put_sample(uint8_t *dst, dmdrvi_audio_format_t format, int16_t sample)
{
    int32_t v = format == DMDRVI_AUDIO_FORMAT_S16_LE ? sample :
                format == DMDRVI_AUDIO_FORMAT_S24_IN32_LE ? (int32_t)sample * 256 : (int32_t)sample * 65536;
    unsigned bytes = format == DMDRVI_AUDIO_FORMAT_S16_LE ? 2U : 4U;
    for (unsigned i = 0; i < bytes; ++i) dst[i] = (uint8_t)((uint32_t)v >> (8U * i));
}

/** The sample as a 16-bit value (the top of what the format carries) */
static int16_t get_sample(const uint8_t *src, dmdrvi_audio_format_t format)
{
    if (format == DMDRVI_AUDIO_FORMAT_S16_LE) return (int16_t)(src[0] | (src[1] << 8));
    int32_t v = (int32_t)((uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24));
    return (int16_t)(format == DMDRVI_AUDIO_FORMAT_S24_IN32_LE ? v / 256 : v / 65536);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/dev/dmsai0";
    unsigned freq = parse(argc > 2 ? argv[2] : NULL, 1000U);
    unsigned seconds = parse(argc > 3 ? argv[3] : NULL, 3U);
    bool loop = argc > 4 && !strcmp(argv[4], "loop");
    void *file = Dmod_FileOpen(path, "r+");
    if (!file) file = Dmod_FileOpen(path, "w");
    if (!file)
    {
        Dmod_Printf("saitone: cannot open %s\n", path);
        return 1;
    }
    int failed = 0;
    dmdrvi_audio_stream_t st;
    memset(&st, 0, sizeof(st));
    int rc = Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STREAM, &st);
    if (rc != 0)
    {
        Dmod_Printf("saitone: GET_STREAM failed (%d)\n", rc);
        Dmod_FileClose(file);
        return 1;
    }
    Dmod_Printf("saitone: %u Hz, %u channels, format %d, slot %u bits, tx=%d rx=%d\n", (unsigned)st.sample_rate_hz, st.channels,
                (int)st.format, st.slot_bits, st.transmit, st.receive);
    uint32_t timeout = 2000;
    Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_SET_TIMEOUT, &timeout);
    unsigned sample_bytes = st.format == DMDRVI_AUDIO_FORMAT_S16_LE ? 2U : 4U;
    unsigned frame_bytes = sample_bytes * st.channels;
    uint8_t *tx = Dmod_Malloc(CHUNK_FRAMES * frame_bytes);
    uint8_t *rx = Dmod_Malloc(CHUNK_FRAMES * frame_bytes);
    if (!tx || !rx)
    {
        Dmod_Printf("saitone: out of memory\n");
        failed = 1;
    }
    else if ((rc = Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_START, NULL)) != 0)
    {
        Dmod_Printf("saitone: START failed (%d)\n", rc);
        failed = 1;
    }
    else
    {
        uint32_t phase = 0, step = (uint32_t)(((uint64_t)freq << 32) / st.sample_rate_hz);
        uint64_t frames_total = (uint64_t)st.sample_rate_hz * seconds, frames_done = 0, rx_frames = 0, rx_quiet = 0;
        int rx_peak = 0;
        while (frames_done < frames_total)
        {
            for (unsigned f = 0; f < CHUNK_FRAMES; ++f)
            {
                int16_t s = (int16_t)(((int32_t)sine64[phase >> 26] * (int32_t)AMPLITUDE_PCT) / 100);
                phase += step;
                for (unsigned c = 0; c < st.channels; ++c) put_sample(tx + f * frame_bytes + c * sample_bytes, st.format, s);
            }
            size_t done = Dmod_FileWrite(tx, 1, CHUNK_FRAMES * frame_bytes, file);
            if (done == 0)
            {
                Dmod_Printf("saitone: write failed after %u frames\n", (unsigned)frames_done);
                failed = 1;
                break;
            }
            frames_done += done / frame_bytes;
            if (loop && st.receive)
            {
                size_t got = Dmod_FileRead(rx, 1, CHUNK_FRAMES * frame_bytes, file);
                for (size_t f = 0; f < got / frame_bytes; ++f)
                {
                    int v = get_sample(rx + f * frame_bytes, st.format);
                    if (v < 0) v = -v;
                    if (v > rx_peak) rx_peak = v;
                    if (v < 64) rx_quiet++;
                    rx_frames++;
                }
            }
        }
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_DRAIN, NULL);
        dmdrvi_audio_status_t status;
        memset(&status, 0, sizeof(status));
        if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STATUS, &status) == 0)
        {
            Dmod_Printf("saitone: actual rate %u Hz, underruns %u, overruns %u, errors %u\n", (unsigned)status.actual_sample_rate_hz,
                        (unsigned)status.tx_underruns, (unsigned)status.rx_overruns, (unsigned)status.transfer_errors);
            if (status.transfer_errors) failed = 1;
        }
        if (loop)
        {
            Dmod_Printf("saitone: received %u frames, peak %d (sent %d), %u of them quiet\n", (unsigned)rx_frames, rx_peak,
                        (int)(32767 * AMPLITUDE_PCT / 100), (unsigned)rx_quiet);
            if (rx_peak < 1000) failed = 1;
        }
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_STOP, NULL);
    }
    if (tx) Dmod_Free(tx);
    if (rx) Dmod_Free(rx);
    Dmod_FileClose(file);
    Dmod_Printf("saitone result: %d\n", failed);
    return failed;
}
