#ifndef PACH_CONFIG_H
#define PACH_CONFIG_H

#include <psptypes.h>
#include <pspctrl.h>

#define PACH_CONFIG_PATH \
    "ms0:/SEPLUGINS/PSPAchievementsNG/config.ini"

#define PACH_CONFIG_DEFAULT_NOTIFICATION_MS 4000u
#define PACH_CONFIG_DEFAULT_STARTUP_MS      2500u
#define PACH_CONFIG_DEFAULT_MENU_HOLD_MS     400u

#define PACH_CONFIG_MIN_NOTIFICATION_MS      250u
#define PACH_CONFIG_MAX_NOTIFICATION_MS    15000u
#define PACH_CONFIG_MIN_MENU_HOLD_MS         100u
#define PACH_CONFIG_MAX_MENU_HOLD_MS        2000u

#define PACH_CONFIG_BUTTON_MASK_DEFAULT \
    (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT)

typedef enum PachPerformanceMode {
    PACH_PERFORMANCE_AUTO = 0,
    PACH_PERFORMANCE_FULL = 1,
    PACH_PERFORMANCE_ADAPTIVE = 2
} PachPerformanceMode;

typedef struct PachConfig {
    int enabled;
    int logging_enabled;

    int notifications_enabled;
    int startup_notification_enabled;
    u32 notification_duration_ms;
    u32 startup_notification_duration_ms;

    int audio_enabled;
    u32 audio_volume_percent;
    int startup_sound_enabled;
    int unlock_sound_enabled;

    int menu_enabled;
    u32 menu_buttons;
    u32 menu_hold_ms;
    int show_badges;

    u8 performance_mode;
    u8 loaded;
    u16 warning_count;
} PachConfig;

enum PachConfigResult {
    PACH_CONFIG_OK = 0,
    PACH_CONFIG_NOT_FOUND = 1,

    PACH_CONFIG_ERROR_ARGUMENT = -11001,
    PACH_CONFIG_ERROR_OPEN = -11002,
    PACH_CONFIG_ERROR_READ = -11003,
    PACH_CONFIG_ERROR_SIZE = -11004
};

void pach_config_reset(
    PachConfig *config
);

int pach_config_load(
    const char *path,
    PachConfig *config
);

u32 pach_config_ms_to_frames(
    u32 milliseconds
);

const char *pach_config_performance_mode_name(
    u8 mode
);

#endif
