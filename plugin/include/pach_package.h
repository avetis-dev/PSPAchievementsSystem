#ifndef PACH_PACKAGE_H
#define PACH_PACKAGE_H

#include <psptypes.h>

#include "pach_achievement.h"
#include "pach_game_detector.h"

#define PACH_PACKAGE_HEADER_SIZE       48u
#define PACH_PACKAGE_ACHIEVEMENT_SIZE  32u
#define PACH_PACKAGE_GROUP_SIZE         8u
#define PACH_PACKAGE_CONDITION_SIZE    20u
#define PACH_PACKAGE_FORMAT_VERSION     3u
#define PACH_PACKAGE_GAME_ID_SIZE      12u
#define PACH_PACKAGE_PATH_CAPACITY     96u

typedef struct PachPackageInfo {
    u16 format_version;
    u16 header_size;

    char game_id[PACH_GAME_ID_CAPACITY];

    u32 achievement_count;
    u32 group_count;
    u32 condition_count;
    u32 string_table_size;
    u32 flags;
    u32 package_id;

    PachAchievementDefinition achievements[PACH_ACHIEVEMENT_MAX];
    PachAchievementGroup groups[PACH_ACHIEVEMENT_GROUP_MAX];
    PachAchievementCondition conditions[PACH_ACHIEVEMENT_CONDITION_MAX];
    char string_table[PACH_ACHIEVEMENT_STRING_MAX];

    int loaded;
} PachPackageInfo;

enum PachPackageResult {
    PACH_PACKAGE_OK = 0,

    PACH_PACKAGE_ERROR_ARGUMENT = -5001,
    PACH_PACKAGE_ERROR_PATH = -5002,
    PACH_PACKAGE_ERROR_READ = -5003,
    PACH_PACKAGE_ERROR_HEADER_SIZE = -5004,
    PACH_PACKAGE_ERROR_MAGIC = -5005,
    PACH_PACKAGE_ERROR_VERSION = -5006,
    PACH_PACKAGE_ERROR_GAME_ID = -5007,
    PACH_PACKAGE_ERROR_GAME_MISMATCH = -5008,
    PACH_PACKAGE_ERROR_COUNT = -5009,
    PACH_PACKAGE_ERROR_ACHIEVEMENT = -5010,
    PACH_PACKAGE_ERROR_ACHIEVEMENT_ID = -5011,
    PACH_PACKAGE_ERROR_GROUP = -5012,
    PACH_PACKAGE_ERROR_CONDITION = -5013,
    PACH_PACKAGE_ERROR_OPERAND = -5014,
    PACH_PACKAGE_ERROR_STRING = -5015,
    PACH_PACKAGE_ERROR_DUPLICATE_ID = -5016,
    PACH_PACKAGE_ERROR_PACKAGE_ID = -5017,
    PACH_PACKAGE_ERROR_CHECKSUM = -5018
};

void pach_package_reset(
    PachPackageInfo *package_info
);

int pach_package_build_path(
    const char *game_id,
    char *output,
    u32 output_capacity
);

int pach_package_load(
    const char *path,
    const char *expected_game_id,
    PachPackageInfo *package_info
);

const char *pach_package_get_string(
    const PachPackageInfo *package_info,
    u32 offset
);

int pach_package_contains_achievement_id(
    const PachPackageInfo *package_info,
    u32 achievement_id
);

#endif
