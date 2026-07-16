#include "pach_logger.h"

#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <psptypes.h>

#define PACH_BASE_DIR "ms0:/SEPLUGINS/PSPAchievementsNG"
#define PACH_LOG_DIR  PACH_BASE_DIR "/logs"
#define PACH_LOG_FILE PACH_LOG_DIR "/plugin.log"

static SceSize pach_string_length(const char *text)
{
    SceSize length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static int pach_write_all(
    SceUID file,
    const char *data,
    SceSize size)
{
    SceSize total_written = 0;

    if (file < 0 || data == NULL) {
        return -1;
    }

    while (total_written < size) {
        int written = sceIoWrite(
            file,
            data + total_written,
            size - total_written
        );

        if (written <= 0) {
            return written < 0 ? written : -1;
        }

        total_written += (SceSize)written;
    }

    return 0;
}

int pach_log_init(void)
{
    static const char header[] =
        "================================\n"
        "PSPAchievementsNG 1.0.0\n"
        "log initialized\n"
        "================================\n";

    SceUID file;
    int result;

    /*
     * Ошибки mkdir игнорируются:
     * каталог может уже существовать.
     */
    sceIoMkdir(PACH_BASE_DIR, 0777);
    sceIoMkdir(PACH_LOG_DIR, 0777);

    file = sceIoOpen(
        PACH_LOG_FILE,
        PSP_O_WRONLY |
        PSP_O_CREAT |
        PSP_O_TRUNC,
        0777
    );

    if (file < 0) {
        return file;
    }

    result = pach_write_all(
        file,
        header,
        sizeof(header) - 1
    );

    sceIoClose(file);

    return result;
}

int pach_log_line(const char *message)
{
    static const char newline[] = "\n";

    SceUID file;
    SceSize message_length;
    int result;

    if (message == NULL) {
        return -1;
    }

    message_length = pach_string_length(message);

    file = sceIoOpen(
        PACH_LOG_FILE,
        PSP_O_WRONLY |
        PSP_O_CREAT |
        PSP_O_APPEND,
        0777
    );

    if (file < 0) {
        return file;
    }

    result = pach_write_all(
        file,
        message,
        message_length
    );

    if (result >= 0) {
        result = pach_write_all(
            file,
            newline,
            sizeof(newline) - 1
        );
    }

    sceIoClose(file);

    return result;
}

int pach_log_key_value(
    const char *key,
    const char *value)
{
    static const char separator[] = ": ";
    static const char newline[] = "\n";

    SceUID file;
    int result;

    if (key == NULL || value == NULL) {
        return -1;
    }

    file = sceIoOpen(
        PACH_LOG_FILE,
        PSP_O_WRONLY |
        PSP_O_CREAT |
        PSP_O_APPEND,
        0777
    );

    if (file < 0) {
        return file;
    }

    result = pach_write_all(
        file,
        key,
        pach_string_length(key)
    );

    if (result >= 0) {
        result = pach_write_all(
            file,
            separator,
            sizeof(separator) - 1
        );
    }

    if (result >= 0) {
        result = pach_write_all(
            file,
            value,
            pach_string_length(value)
        );
    }

    if (result >= 0) {
        result = pach_write_all(
            file,
            newline,
            sizeof(newline) - 1
        );
    }

    sceIoClose(file);

    return result;
}

static char pach_hex_digit(u32 value)
{
    value &= 0x0Fu;

    if (value < 10u) {
        return (char)('0' + value);
    }

    return (char)('A' + (value - 10u));
}

int pach_log_hex32(
    const char *key,
    u32 value)
{
    char text[11];
    int index;

    if (key == NULL) {
        return -1;
    }

    text[0] = '0';
    text[1] = 'x';

    for (index = 0; index < 8; ++index) {
        u32 shift =
            (u32)(7 - index) * 4u;

        text[index + 2] =
            pach_hex_digit(value >> shift);
    }

    text[10] = '\0';

    return pach_log_key_value(
        key,
        text
    );
}