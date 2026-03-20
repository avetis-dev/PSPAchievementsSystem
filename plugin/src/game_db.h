#ifndef GAME_DB_H
#define GAME_DB_H

#include "format.h"

typedef struct {
    PACH_GameFileHeader header;
    PACH_AchievementDef achievements[PACH_MAX_GAME_ACH];
    int loaded;
} PACH_LoadedGame;

int pach_game_load_file(PACH_LoadedGame *game, const char *path);
void pach_game_clear(PACH_LoadedGame *game);
PACH_AchievementDef *pach_game_get_achievement(PACH_LoadedGame *game, int index);

#endif /* GAME_DB_H */