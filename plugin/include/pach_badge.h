#ifndef PACH_BADGE_H
#define PACH_BADGE_H

#include <psptypes.h>

#include "pach_achievement.h"
#include "pach_game_detector.h"

#define PACH_BADGE_FORMAT_VERSION 1u
#define PACH_BADGE_HEADER_SIZE 48u
#define PACH_BADGE_RECORD_SIZE 12u
#define PACH_BADGE_WIDTH 32u
#define PACH_BADGE_HEIGHT 32u
#define PACH_BADGE_PIXEL_FORMAT_RGB565 1u
#define PACH_BADGE_PIXELS_PER_IMAGE \
    (PACH_BADGE_WIDTH * PACH_BADGE_HEIGHT)
#define PACH_BADGE_PIXEL_CAPACITY \
    (PACH_ACHIEVEMENT_MAX * PACH_BADGE_PIXELS_PER_IMAGE)
#define PACH_BADGE_PATH_CAPACITY 96u

typedef struct PachBadgeRecord {
    u32 achievement_id;
    u32 pixel_offset;
    u32 pixel_size;
} PachBadgeRecord;

typedef struct PachBadgePack {
    u16 format_version;
    u16 header_size;

    char game_id[PACH_GAME_ID_CAPACITY];

    u16 width;
    u16 height;
    u16 pixel_format;
    u16 record_size;

    u32 badge_count;
    u32 pixel_data_size;
    u32 pack_id;

    PachBadgeRecord records[PACH_ACHIEVEMENT_MAX];
    u16 pixels[PACH_BADGE_PIXEL_CAPACITY];

    int loaded;
} PachBadgePack;

enum PachBadgeResult {
    PACH_BADGE_OK = 0,
    PACH_BADGE_NOT_FOUND = 1,

    PACH_BADGE_ERROR_ARGUMENT = -10001,
    PACH_BADGE_ERROR_PATH = -10002,
    PACH_BADGE_ERROR_OPEN = -10003,
    PACH_BADGE_ERROR_READ = -10004,
    PACH_BADGE_ERROR_MAGIC = -10005,
    PACH_BADGE_ERROR_VERSION = -10006,
    PACH_BADGE_ERROR_HEADER = -10007,
    PACH_BADGE_ERROR_GAME_ID = -10008,
    PACH_BADGE_ERROR_GAME_MISMATCH = -10009,
    PACH_BADGE_ERROR_COUNT = -10010,
    PACH_BADGE_ERROR_FORMAT = -10011,
    PACH_BADGE_ERROR_RECORD = -10012,
    PACH_BADGE_ERROR_DUPLICATE = -10013,
    PACH_BADGE_ERROR_CHECKSUM = -10014,
    PACH_BADGE_ERROR_PACK_ID = -10015
};

void pach_badge_reset(
    PachBadgePack *pack
);

int pach_badge_build_path(
    const char *game_id,
    char *output,
    u32 output_capacity
);

int pach_badge_load(
    const char *path,
    const char *expected_game_id,
    PachBadgePack *pack
);

const u16 *pach_badge_get_pixels(
    const PachBadgePack *pack,
    u32 achievement_id
);

#endif
