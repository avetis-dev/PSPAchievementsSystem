#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>

#include "game_map.h"

void pach_gamemap_clear(PACH_GameMapDb *db)
{
    if (!db) return;
    memset(db, 0, sizeof(PACH_GameMapDb));
}

int pach_gamemap_load(PACH_GameMapDb *db, const char *path)
{
    SceUID fd;
    int read_bytes;
    int entry_bytes;
    int count = 0;

    if (!db || !path || !path[0]) return 0;

    pach_gamemap_clear(db);

    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return 0;

    read_bytes = sceIoRead(fd, &count, sizeof(int));
    if (read_bytes != sizeof(int)) {
        sceIoClose(fd);
        return 0;
    }

    if (count < 0 || count > 128) {
        sceIoClose(fd);
        return 0;
    }

    entry_bytes = sizeof(PACH_GameMapEntry) * count;
    read_bytes = sceIoRead(fd, db->entries, entry_bytes);
    sceIoClose(fd);

    if (read_bytes != entry_bytes) {
        pach_gamemap_clear(db);
        return 0;
    }

    db->count = count;
    db->loaded = 1;
    return 1;
}

PACH_GameMapEntry *pach_gamemap_find_by_code(PACH_GameMapDb *db, const char *game_code)
{
    if (!db || !db->loaded || !game_code) return 0;

    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->entries[i].game_code, game_code) == 0)
            return &db->entries[i];
    }

    return 0;
}