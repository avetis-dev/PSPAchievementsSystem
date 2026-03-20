#ifndef GAME_MAP_H
#define GAME_MAP_H

#include "format.h"

typedef struct {
    PACH_GameMapEntry entries[128];
    int count;
    int loaded;
} PACH_GameMapDb;

int pach_gamemap_load(PACH_GameMapDb *db, const char *path);
void pach_gamemap_clear(PACH_GameMapDb *db);
PACH_GameMapEntry *pach_gamemap_find_by_code(PACH_GameMapDb *db, const char *game_code);

#endif /* GAME_MAP_H */
