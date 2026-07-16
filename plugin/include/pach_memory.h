#ifndef PACH_MEMORY_H
#define PACH_MEMORY_H

#include <psptypes.h>

#define PACH_MEMORY_SIZE_32_MB 0x02000000u

#define PACH_USER_MEMORY_BASE   0x08000000u
#define PACH_KERNEL_MEMORY_BASE 0x88000000u

typedef enum PachAddressKind {
    PACH_ADDRESS_OFFSET = 0,
    PACH_ADDRESS_USER   = 1,
    PACH_ADDRESS_KERNEL = 2
} PachAddressKind;

typedef struct PachMemoryMap {
    u32 ram_size;
    s32 relocation;
} PachMemoryMap;

enum PachMemoryResult {
    PACH_MEMORY_OK = 0,

    PACH_MEMORY_ERROR_ARGUMENT = -2001,
    PACH_MEMORY_ERROR_KIND     = -2002,
    PACH_MEMORY_ERROR_RANGE    = -2003,
    PACH_MEMORY_ERROR_TEST     = -2004
};

void pach_memory_map_init(
    PachMemoryMap *map
);

int pach_memory_translate(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 access_size,
    u32 *kernel_address
);

int pach_memory_read_bytes(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u8 *buffer,
    u32 buffer_size
);

int pach_memory_read_u8(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u8 *value
);

int pach_memory_read_u16_le(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u16 *value
);

int pach_memory_read_u24_le(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value
);

int pach_memory_read_u32_le(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value
);

int pach_memory_read_u16_be(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u16 *value
);

int pach_memory_read_u24_be(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value
);

int pach_memory_read_u32_be(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value
);

int pach_memory_self_test(
    const PachMemoryMap *map
);

#endif
