#include "pach_game_detector.h"

#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>
#include <psptypes.h>

#define PACH_UMD_DATA_PATH "disc0:/UMD_DATA.BIN"

#define PACH_GAME_ERROR_INVALID_ARGUMENT (-1001)
#define PACH_GAME_ERROR_SHORT_READ       (-1002)
#define PACH_GAME_ERROR_INVALID_FORMAT   (-1003)

static int pach_is_ascii_letter(char value)
{
    return
        (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z');
}

static int pach_is_ascii_digit(char value)
{
    return value >= '0' && value <= '9';
}

static char pach_to_ascii_upper(char value)
{
    if (value >= 'a' && value <= 'z') {
        return (char)(value - ('a' - 'A'));
    }

    return value;
}

static int pach_validate_game_id(const char *game_id)
{
    int index;

    if (game_id == NULL) {
        return 0;
    }

    /*
     * Стандартный формат PSP:
     *
     * ULUS-10285
     * ULES-00869
     * ULJM-05281
     */
    for (index = 0; index < 4; ++index) {
        if (!pach_is_ascii_letter(game_id[index])) {
            return 0;
        }
    }

    if (game_id[4] != '-') {
        return 0;
    }

    for (index = 5; index < PACH_GAME_ID_LENGTH; ++index) {
        if (!pach_is_ascii_digit(game_id[index])) {
            return 0;
        }
    }

    return 1;
}

int pach_game_detect(
    char game_id[PACH_GAME_ID_CAPACITY])
{
    SceUID file;
    int bytes_read;
    int index;

    if (game_id == NULL) {
        return PACH_GAME_ERROR_INVALID_ARGUMENT;
    }

    game_id[0] = '\0';

    file = sceIoOpen(
        PACH_UMD_DATA_PATH,
        PSP_O_RDONLY,
        0777
    );

    if (file < 0) {
        /*
         * Возвращаем оригинальный код ошибки PSP,
         * чтобы позже можно было его диагностировать.
         */
        return file;
    }

    bytes_read = sceIoRead(
        file,
        game_id,
        PACH_GAME_ID_LENGTH
    );

    sceIoClose(file);

    if (bytes_read < 0) {
        game_id[0] = '\0';
        return bytes_read;
    }

    if (bytes_read != PACH_GAME_ID_LENGTH) {
        game_id[0] = '\0';
        return PACH_GAME_ERROR_SHORT_READ;
    }

    game_id[PACH_GAME_ID_LENGTH] = '\0';

    for (index = 0; index < 4; ++index) {
        game_id[index] = pach_to_ascii_upper(game_id[index]);
    }

    if (!pach_validate_game_id(game_id)) {
        game_id[0] = '\0';
        return PACH_GAME_ERROR_INVALID_FORMAT;
    }

    return 0;
}