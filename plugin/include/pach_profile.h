#ifndef PACH_PROFILE_H
#define PACH_PROFILE_H

#include <psptypes.h>

#include "pach_achievement.h"
#include "pach_game_detector.h"

#define PACH_PROFILE_FORMAT_VERSION 1u
#define PACH_PROFILE_HEADER_SIZE    32u
#define PACH_PROFILE_PATH_CAPACITY  112u

typedef struct PachProfilePaths {
    char main_path[PACH_PROFILE_PATH_CAPACITY];
    char temp_path[PACH_PROFILE_PATH_CAPACITY];
    char backup_path[PACH_PROFILE_PATH_CAPACITY];
} PachProfilePaths;

typedef struct PachProfile {
    char game_id[PACH_GAME_ID_CAPACITY];

    u32 package_id;
    u32 unlocked_count;
    u32 unlocked_ids[PACH_ACHIEVEMENT_MAX];

    int loaded;
    int dirty;
    int recovered_from_backup;
    int primary_error;
} PachProfile;

enum PachProfileResult {
    PACH_PROFILE_OK        = 0,
    PACH_PROFILE_NOT_FOUND = 1,
    PACH_PROFILE_ADDED     = 2,
    PACH_PROFILE_EXISTS    = 3,
    PACH_PROFILE_PACKAGE_MISMATCH = 4,

    PACH_PROFILE_ERROR_ARGUMENT         = -7001,
    PACH_PROFILE_ERROR_PATH             = -7002,
    PACH_PROFILE_ERROR_OPEN             = -7003,
    PACH_PROFILE_ERROR_READ             = -7004,
    PACH_PROFILE_ERROR_WRITE            = -7005,
    PACH_PROFILE_ERROR_MAGIC            = -7006,
    PACH_PROFILE_ERROR_VERSION          = -7007,
    PACH_PROFILE_ERROR_HEADER_SIZE      = -7008,
    PACH_PROFILE_ERROR_GAME_ID          = -7009,
    PACH_PROFILE_ERROR_GAME_MISMATCH    = -7010,
    PACH_PROFILE_ERROR_COUNT            = -7012,
    PACH_PROFILE_ERROR_DUPLICATE_ID     = -7013,
    PACH_PROFILE_ERROR_CHECKSUM         = -7014,
    PACH_PROFILE_ERROR_RENAME           = -7015,
    PACH_PROFILE_ERROR_VERIFY           = -7016
};

void pach_profile_reset(
    PachProfile *profile
);

int pach_profile_build_paths(
    const char *game_id,
    PachProfilePaths *paths
);

int pach_profile_init_new(
    PachProfile *profile,
    const char *game_id,
    u32 package_id
);

int pach_profile_load(
    const PachProfilePaths *paths,
    const char *expected_game_id,
    u32 expected_package_id,
    PachProfile *profile
);

int pach_profile_add_unlocked(
    PachProfile *profile,
    u32 achievement_id
);

int pach_profile_save_atomic(
    const PachProfilePaths *paths,
    PachProfile *profile
);

#endif
