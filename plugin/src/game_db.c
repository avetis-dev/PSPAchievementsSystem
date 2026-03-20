#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>

#include "game_db.h"

/* Logging function defined in main.c (external */
#define LOG_PATH "ms0:/PSP/ACH/pach_log.txt"
static void db_log(const char *msg) {
    SceUID fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, msg, strlen(msg));
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
}
static void db_log_int(const char *prefix, int val) {
    char buf[128], num[16];
    int i = 0, pos = 0, n = val < 0 ? -val : val;
    if (n == 0) num[i++] = '0';
    else while (n > 0) { num[i++] = '0' + (n % 10); n /= 10; }
    while (*prefix && pos < 100) buf[pos++] = *prefix++;
    if (val < 0) buf[pos++] = '-';
    for (int j = i - 1; j >= 0; j--) buf[pos++] = num[j];
    buf[pos] = '\0';
    db_log(buf);
}

void pach_game_clear(PACH_LoadedGame *game) {
    if (!game) return;
    memset(game, 0, sizeof(PACH_LoadedGame));
}

int pach_game_load_file(PACH_LoadedGame *game, const char *path) {
    SceUID fd;
    int read_bytes, ach_bytes;

    db_log("game_db: opening file");
    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) { db_log("game_db: file open failed"); return 0; }

    db_log("game_db: reading header");
    read_bytes = sceIoRead(fd, &game->header, sizeof(PACH_GameFileHeader));
    db_log_int("game_db: read header bytes = ", read_bytes);
    db_log_int("game_db: expected header bytes = ", sizeof(PACH_GameFileHeader));

    if (read_bytes != sizeof(PACH_GameFileHeader)) {
        sceIoClose(fd); pach_game_clear(game); return 0;
    }

    if (game->header.magic[0] != PACH_GAME_MAGIC_0 ||
        game->header.magic[1] != PACH_GAME_MAGIC_1 ||
        game->header.magic[2] != PACH_GAME_MAGIC_2 ||
        game->header.magic[3] != PACH_GAME_MAGIC_3) {
        db_log("game_db: magic mismatch");
        sceIoClose(fd); pach_game_clear(game); return 0;
    }

    db_log_int("game_db: version = ", game->header.version);
    if (game->header.version != PACH_VERSION) {
        db_log("game_db: version mismatch");
        sceIoClose(fd); pach_game_clear(game); return 0;
    }

    db_log_int("game_db: num_achievements = ", game->header.num_achievements);
    if (game->header.num_achievements < 0 || game->header.num_achievements > PACH_MAX_GAME_ACH) {
        db_log("game_db: ach count out of bounds");
        sceIoClose(fd); pach_game_clear(game); return 0;
    }

    ach_bytes = sizeof(PACH_AchievementDef) * game->header.num_achievements;
    db_log_int("game_db: expected ach bytes = ", ach_bytes);
    
    read_bytes = sceIoRead(fd, game->achievements, ach_bytes);
    db_log_int("game_db: read ach bytes = ", read_bytes);
    
    sceIoClose(fd);

    if (read_bytes != ach_bytes) {
        db_log("game_db: ach bytes mismatch");
        pach_game_clear(game); return 0;
    }

    db_log("game_db: LOAD SUCCESS");
    game->loaded = 1;
    return 1;
}

PACH_AchievementDef *pach_game_get_achievement(PACH_LoadedGame *game, int index) {
    if (!game || !game->loaded) return 0;
    if (index < 0 || index >= game->header.num_achievements) return 0;
    return &game->achievements[index];
}