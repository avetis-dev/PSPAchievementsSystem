#ifndef PACH_MEMORY_WATCH_H
#define PACH_MEMORY_WATCH_H

#include "pach_memory.h"

typedef enum PachMemoryWidth {
    PACH_MEMORY_WIDTH_8  = 1,
    PACH_MEMORY_WIDTH_16 = 2,
    PACH_MEMORY_WIDTH_24 = 3,
    PACH_MEMORY_WIDTH_32 = 4
} PachMemoryWidth;

typedef struct PachMemoryWatch {
    u32 address;
    PachAddressKind address_kind;
    PachMemoryWidth width;

    u32 current_value;
    u32 previous_value;
    u32 change_count;

    int initialized;
} PachMemoryWatch;

enum PachMemoryWatchResult {
    PACH_MEMORY_WATCH_UNCHANGED   = 0,
    PACH_MEMORY_WATCH_INITIALIZED = 1,
    PACH_MEMORY_WATCH_CHANGED     = 2,

    PACH_MEMORY_WATCH_ERROR_ARGUMENT = -4001,
    PACH_MEMORY_WATCH_ERROR_WIDTH    = -4002
};

void pach_memory_watch_reset(
    PachMemoryWatch *watch
);

int pach_memory_watch_init(
    PachMemoryWatch *watch,
    u32 address,
    PachAddressKind address_kind,
    PachMemoryWidth width
);

int pach_memory_watch_sample(
    PachMemoryWatch *watch,
    const PachMemoryMap *memory_map
);

int pach_memory_watch_rebase(
    PachMemoryWatch *watch
);

#endif