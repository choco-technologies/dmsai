#include "dmod.h"
#include "dmsai_ioctl.h"
#include <errno.h>
#include <stdint.h>

#define TEST_FRAMES 300U
#define TEST_FRAME_BYTES 4U

static uint8_t tx[TEST_FRAMES * TEST_FRAME_BYTES];
static uint8_t rx[TEST_FRAME_BYTES];

/** @brief Exercise the public file and ioctl path through dmdevfs. */
static int exercise_device(void *file, const char *path)
{
    dmsai_config_t config;
    dmsai_status_t status;
    uint32_t timeout = 0;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_GET_CONFIG, &config) != 0 ||
        Dmod_Ioctl(file, DMSAI_IOCTL_GET_STATUS, &status) != 0 ||
        status.running || !config.transmit || !config.receive ||
        config.pcm_format != dmsai_pcm_s16_le ||
        __builtin_popcount(config.active_slots) != 2)
        return -1;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_GET_IO_TIMEOUT, &timeout) != 0 || timeout)
        return -2;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_GET_CONFIG, NULL) != -EINVAL ||
        Dmod_Ioctl(file, DMSAI_IOCTL_DRAIN + 1, NULL) != -ENOTTY)
        return -12;
    timeout = 500U;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_SET_IO_TIMEOUT, &timeout) != 0 ||
        Dmod_Ioctl(file, DMSAI_IOCTL_GET_IO_TIMEOUT, &timeout) != 0 ||
        timeout != 500U) return -3;
    void *second = Dmod_FileOpen(path, "r+");
    if (second) { Dmod_FileClose(second); return -4; }
    if (Dmod_Ioctl(file, DMSAI_IOCTL_START, NULL) != 0 ||
        Dmod_Ioctl(file, DMSAI_IOCTL_START, NULL) != -EBUSY) return -5;
    for (unsigned i = 0; i < sizeof(tx); ++i) tx[i] = (uint8_t)i;
    if (Dmod_FileWrite(tx, 1, sizeof(tx), file) != sizeof(tx)) return -6;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_DRAIN, NULL) != 0) return -7;
    if (Dmod_FileRead(rx, 1, sizeof(rx), file) != sizeof(rx)) return -8;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_GET_STATUS, &status) != 0 ||
        !status.running || status.transfer_errors) return -9;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_STOP, NULL) != 0 ||
        Dmod_Ioctl(file, DMSAI_IOCTL_STOP, NULL) != 0 ||
        Dmod_Ioctl(file, DMSAI_IOCTL_GET_STATUS, &status) != 0 ||
        status.running) return -10;
    if (Dmod_Ioctl(file, DMSAI_IOCTL_DRAIN, NULL) != -EPIPE)
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
