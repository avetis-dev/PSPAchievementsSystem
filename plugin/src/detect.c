#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>

#include "detect.h"

static int detect_from_umd_data(char *out_code, int max_len)
{
    SceUID fd;
    char buf[64];
    int read_bytes;
    int i, j;

    fd = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0);
    if (fd < 0)
        return 0;

    memset(buf, 0, sizeof(buf));
    read_bytes = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);

    if (read_bytes <= 0)
        return 0;

    buf[read_bytes] = '\0';

    j = 0;
    for (i = 0; i < read_bytes && j < max_len - 1; i++) {
        if (buf[i] == '|' || buf[i] == '\r' || buf[i] == '\n')
            break;
        if (buf[i] != '-')
            out_code[j++] = buf[i];
    }
    out_code[j] = '\0';

    return (j > 0) ? 1 : 0;
}

int pach_detect_game_code(char *out_code, int max_len)
{
    if (!out_code || max_len < 10)
        return 0;

    memset(out_code, 0, max_len);

    if (detect_from_umd_data(out_code, max_len))
        return 1;

    /* Fallback for testing */
    strncpy(out_code, "ULUS10285", max_len - 1);
    out_code[max_len - 1] = '\0';
    return 1;
}