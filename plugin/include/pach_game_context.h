#ifndef PACH_GAME_CONTEXT_H
#define PACH_GAME_CONTEXT_H

#include "pach_game_detector.h"
#include "pach_memory.h"

typedef struct PachGameContext {
    char game_id[PACH_GAME_ID_CAPACITY];

    PachMemoryMap memory_map;

    int initialized;
} PachGameContext;

enum PachGameContextResult {
    PACH_GAME_CONTEXT_OK = 0,

    PACH_GAME_CONTEXT_ERROR_ARGUMENT = -3001,
    PACH_GAME_CONTEXT_ERROR_GAME_ID  = -3002
};

int pach_game_context_init(
    PachGameContext *context,
    const char *game_id
);

void pach_game_context_reset(
    PachGameContext *context
);

#endif