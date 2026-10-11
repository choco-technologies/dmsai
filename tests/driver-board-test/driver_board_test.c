#include "dmod.h"
#include "dmdrvi_ioctl.h"
#include <errno.h>
#include <stdint.h>

#define TEST_FRAMES 300U
#define TEST_FRAME_BYTES 4U

static uint8_t tx[TEST_FRAMES * TEST_FRAME_BYTES];
static uint8_t rx[TEST_FRAME_BYTES];

/** @brief Exercise the public file and ioctl path through dmdevfs. */
static int exercise_device(void *file, const char *path)
{
    dmdrvi_audio_stream_t config;
    dmdrvi_audio_status_t status;
    uint32_t timeout = 0;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STREAM, &config) != 0 ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STATUS, &status) != 0 ||
        status.running || !config.transmit || !config.receive ||
        config.format != DMDRVI_AUDIO_FORMAT_S16_LE ||
        config.channels != 2)
        return -1;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_TIMEOUT, &timeout) != 0 || timeout)
        return -2;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STREAM, NULL) != -EINVAL ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_CUSTOM_BASE, NULL) != -ENOTTY)
        return -12;
    timeout = 500U;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_SET_TIMEOUT, &timeout) != 0 ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_TIMEOUT, &timeout) != 0 ||
        timeout != 500U) return -3;
    void *second = Dmod_FileOpen(path, "r+");
    if (second) { Dmod_FileClose(second); return -4; }
    dmdrvi_audio_config_t format = { .sample_rate_hz = config.sample_rate_hz,
                                     .channels = config.channels, .sample_bits = 16 };
    uint8_t volume = 50;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_CONFIGURE, &format) != 0 ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_SET_VOLUME, &volume) != -ENOTSUP)
        return -14;
    format.sample_rate_hz += 1U;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_CONFIGURE, &format) != -ENOTSUP)
        return -15;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_START, NULL) != 0 ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_START, NULL) != 0) return -5;
    for (unsigned i = 0; i < sizeof(tx); ++i) tx[i] = (uint8_t)i;
    if (Dmod_FileWrite(tx, 1, sizeof(tx), file) != sizeof(tx)) return -6;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_DRAIN, NULL) != 0) return -7;
    if (Dmod_FileRead(rx, 1, sizeof(rx), file) != sizeof(rx)) return -8;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STATUS, &status) != 0 ||
        !status.running || status.transfer_errors) return -9;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_STOP, NULL) != 0 ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_STOP, NULL) != 0 ||
        Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_GET_STATUS, &status) != 0 ||
        status.running) return -10;
    if (Dmod_Ioctl(file, DMDRVI_IOCTL_AUDIO_DRAIN, NULL) != -EPIPE)
        return -13;
    return 0;
}

/** @brief Run the device-node test after dmdevfs mounted an SAI config. */
int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/dev/dmsai1";
    void *file = Dmod_FileOpen(path, "r+");
    if (!file) {
        Dmod_Printf("DMSAI DRIVER BOARD TEST: cannot open %s\n", path);
        return 1;
    }
    int rc = exercise_device(file, path);
    Dmod_FileClose(file);
    if (rc == 0) {
        file = Dmod_FileOpen(path, "r+");
        if (!file) rc = -11;
        else Dmod_FileClose(file);
    }
    Dmod_Printf("DMSAI DRIVER BOARD TEST: %s (%d)\n",
                rc == 0 ? "PASS" : "FAIL", rc);
    return rc == 0 ? 0 : 1;
}
