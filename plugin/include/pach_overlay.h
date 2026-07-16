#ifndef PACH_OVERLAY_H
#define PACH_OVERLAY_H

#include <psptypes.h>

#include "pach_badge.h"
#include "pach_config.h"
#include "pach_notification.h"
#include "pach_package.h"
#include "pach_profile.h"
#include "pach_sound.h"

#define PACH_OVERLAY_STATUS_FRAMES      150u
#define PACH_OVERLAY_ACHIEVEMENT_FRAMES 240u

#define PACH_OVERLAY_MENU_HOLD_FRAMES 24u
#define PACH_OVERLAY_MENU_VISIBLE_ROWS 7u
#define PACH_OVERLAY_MAX_SUSPENDED_THREADS 128u

typedef enum PachOverlayMenuFilter {
    PACH_OVERLAY_FILTER_ALL = 0,
    PACH_OVERLAY_FILTER_LOCKED = 1,
    PACH_OVERLAY_FILTER_UNLOCKED = 2
} PachOverlayMenuFilter;

typedef struct PachOverlay {
    PachNotificationQueue *queue;
    volatile int *running;
    const PachConfig *config;

    const PachPackageInfo *package_info;
    const PachBadgePack *badge_pack;
    const volatile PachAchievementEngine *achievement_engine;
    volatile PachProfile *profile;
    volatile int *profile_ready;

    PachNotification active_notification;
    PachSound sound;

    u32 frames_remaining;
    u32 previous_buttons;
    u32 menu_hold_frames;
    u32 menu_selected_index;
    u32 menu_first_visible;
    u32 menu_filter;

    SceUID suspended_threads[PACH_OVERLAY_MAX_SUSPENDED_THREADS];
    u32 suspended_thread_count;

    int sound_ready;
    int sound_pending;
    int active;
    int game_paused;
    volatile int menu_open;
    int initialized;
} PachOverlay;

enum PachOverlayResult {
    PACH_OVERLAY_OK = 0,

    PACH_OVERLAY_ERROR_ARGUMENT = -9001,
    PACH_OVERLAY_ERROR_DISPLAY  = -9002,
    PACH_OVERLAY_ERROR_FORMAT   = -9003
};

void pach_overlay_reset(
    PachOverlay *overlay
);

int pach_overlay_init(
    PachOverlay *overlay,
    PachNotificationQueue *queue,
    volatile int *running,
    const PachConfig *config,
    const PachPackageInfo *package_info,
    const PachBadgePack *badge_pack,
    const volatile PachAchievementEngine *achievement_engine,
    volatile PachProfile *profile,
    volatile int *profile_ready
);

void pach_overlay_prepare_stop(
    PachOverlay *overlay
);

int pach_overlay_run(
    PachOverlay *overlay
);

#endif
