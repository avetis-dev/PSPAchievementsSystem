#include <pspkernel.h>
#include <pspthreadman.h>

#include <stddef.h>
#include "pach_achievement.h"
#include "pach_badge.h"
#include "pach_config.h"
#include "pach_game_context.h"
#include "pach_game_detector.h"
#include "pach_logger.h"
#include "pach_memory.h"
#include "pach_memory_watch.h"
#include "pach_notification.h"
#include "pach_overlay.h"
#include "pach_package.h"
#include "pach_profile.h"

PSP_MODULE_INFO(
    "PSPAchievementsNG",
    PSP_MODULE_KERNEL,
    1,
    0
);

PSP_MAIN_THREAD_ATTR(0);

#define PACH_IDLE_TICK_INTERVAL_US  (250 * 1000)
#define PACH_ENGINE_TICK_INTERVAL_US 16667
#define PACH_GAME_DETECT_MAX_TRIES  40
#define PACH_ALIVE_TICK             8
#define PACH_EXECUTABLE_STABLE_SAMPLES 12u
#define PACH_THREAD_STOP_TIMEOUT_US 500000u

#define PACH_MEMORY_PROBE_RA_ADDRESS 0x00804000u
#define PACH_PSP_HEADER_MAGIC        0x5053507Eu
#define PACH_ELF_HEADER_MAGIC        0x464C457Fu

typedef enum PachExecutableState {
    PACH_EXECUTABLE_WAITING = 0,
    PACH_EXECUTABLE_STABILIZING,
    PACH_EXECUTABLE_READY
} PachExecutableState;

static volatile int g_running = 0;
static volatile int g_logger_ready = 0;
static int g_module_active = 0;
static int g_config_result = PACH_CONFIG_NOT_FOUND;

static PachConfig g_config;

static SceUID g_worker_thread_id = -1;
static SceUID g_overlay_thread_id = -1;

static PachGameContext g_game_context;
static PachMemoryWatch g_probe_watch;
static PachPackageInfo g_package_info;
static PachBadgePack g_badge_pack;
static PachProfile g_profile;
static PachProfilePaths g_profile_paths;

static PachNotificationQueue g_notification_queue;
static PachOverlay g_overlay;

static PachAchievementEngine g_achievement_engine;
static PachAchievementEvent g_achievement_events[PACH_ACHIEVEMENT_MAX];

static char g_package_path[PACH_PACKAGE_PATH_CAPACITY];
static char g_badge_path[PACH_BADGE_PATH_CAPACITY];

static PachExecutableState g_executable_state =
    PACH_EXECUTABLE_WAITING;

static u32 g_executable_candidate = 0;
static u32 g_executable_stable_samples = 0;

static int g_package_attempted = 0;
static int g_achievement_engine_failed = 0;
static int g_profile_ready = 0;
static int g_performance_logged = 0;

static int pach_feedback_needed(
    int startup)
{
    if (startup) {
        return
            (g_config.notifications_enabled &&
             g_config.startup_notification_enabled) ||
            (g_config.audio_enabled &&
             g_config.startup_sound_enabled &&
             g_config.audio_volume_percent > 0);
    }

    return
        g_config.notifications_enabled ||
        (g_config.audio_enabled &&
         g_config.unlock_sound_enabled &&
         g_config.audio_volume_percent > 0);
}

static u8 pach_config_scheduler_mode(void)
{
    if (g_config.performance_mode ==
        PACH_PERFORMANCE_FULL) {

        return PACH_ACHIEVEMENT_SCHEDULER_FULL;
    }

    if (g_config.performance_mode ==
        PACH_PERFORMANCE_ADAPTIVE) {

        return PACH_ACHIEVEMENT_SCHEDULER_ADAPTIVE;
    }

    return PACH_ACHIEVEMENT_SCHEDULER_AUTO;
}

static void pach_log_configuration(void)
{
    if (!g_logger_ready) {
        return;
    }

    if (g_config_result == PACH_CONFIG_NOT_FOUND) {
        pach_log_line("config not found; defaults active");
    } else if (g_config_result < 0) {
        pach_log_hex32("config load error", (u32)g_config_result);
        pach_log_line("config defaults active");
    } else {
        pach_log_line("config loaded");
    }

    pach_log_key_value(
        "config path",
        PACH_CONFIG_PATH
    );

    pach_log_hex32(
        "config warning count",
        g_config.warning_count
    );

    pach_log_hex32(
        "notification duration ms",
        g_config.notification_duration_ms
    );

    pach_log_hex32(
        "audio volume percent",
        g_config.audio_volume_percent
    );

    pach_log_hex32(
        "menu button mask",
        g_config.menu_buttons
    );

    pach_log_key_value(
        "performance mode",
        pach_config_performance_mode_name(
            g_config.performance_mode
        )
    );
}

static int pach_is_loader_magic(u32 value)
{
    return
        value == PACH_PSP_HEADER_MAGIC ||
        value == PACH_ELF_HEADER_MAGIC;
}

static int pach_probe_game_memory(
    const PachMemoryMap *memory_map,
    u32 *kernel_address,
    u32 *probe_value)
{
    u8 value8;
    u16 value16;
    u32 value24;
    u32 value32;

    int result;

    if (memory_map == NULL ||
        kernel_address == NULL ||
        probe_value == NULL) {

        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *kernel_address = 0;
    *probe_value = 0;

    result = pach_memory_translate(
        memory_map,
        PACH_MEMORY_PROBE_RA_ADDRESS,
        PACH_ADDRESS_OFFSET,
        4,
        kernel_address
    );

    if (result < 0) {
        return result;
    }

    result = pach_memory_read_u8(
        memory_map,
        PACH_MEMORY_PROBE_RA_ADDRESS,
        PACH_ADDRESS_OFFSET,
        &value8
    );

    if (result < 0) {
        return result;
    }

    result = pach_memory_read_u16_le(
        memory_map,
        PACH_MEMORY_PROBE_RA_ADDRESS,
        PACH_ADDRESS_OFFSET,
        &value16
    );

    if (result < 0) {
        return result;
    }

    result = pach_memory_read_u24_le(
        memory_map,
        PACH_MEMORY_PROBE_RA_ADDRESS,
        PACH_ADDRESS_OFFSET,
        &value24
    );

    if (result < 0) {
        return result;
    }

    result = pach_memory_read_u32_le(
        memory_map,
        PACH_MEMORY_PROBE_RA_ADDRESS,
        PACH_ADDRESS_OFFSET,
        &value32
    );

    if (result < 0) {
        return result;
    }

    if ((value32 & 0x000000FFu) != (u32)value8) {
        return PACH_MEMORY_ERROR_TEST;
    }

    if ((value32 & 0x0000FFFFu) != (u32)value16) {
        return PACH_MEMORY_ERROR_TEST;
    }

    if ((value32 & 0x00FFFFFFu) != value24) {
        return PACH_MEMORY_ERROR_TEST;
    }

    *probe_value = value32;
    return PACH_MEMORY_OK;
}

static int pach_initialize_game(
    const char *game_id,
    u32 *probe_kernel_address,
    u32 *probe_value)
{
    int result;

    if (game_id == NULL ||
        probe_kernel_address == NULL ||
        probe_value == NULL) {

        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    result = pach_game_context_init(
        &g_game_context,
        game_id
    );

    if (result < 0) {
        return result;
    }

    result = pach_memory_self_test(
        &g_game_context.memory_map
    );

    if (result < 0) {
        pach_game_context_reset(&g_game_context);
        return result;
    }

    result = pach_probe_game_memory(
        &g_game_context.memory_map,
        probe_kernel_address,
        probe_value
    );

    if (result < 0) {
        pach_game_context_reset(&g_game_context);
        return result;
    }

    return PACH_MEMORY_OK;
}

static int pach_initialize_probe_watch(void)
{
    int result;

    pach_memory_watch_reset(&g_probe_watch);

    result = pach_memory_watch_init(
        &g_probe_watch,
        PACH_MEMORY_PROBE_RA_ADDRESS,
        PACH_ADDRESS_OFFSET,
        PACH_MEMORY_WIDTH_32
    );

    if (result < 0) {
        return result;
    }

    result = pach_memory_watch_sample(
        &g_probe_watch,
        &g_game_context.memory_map
    );

    if (result != PACH_MEMORY_WATCH_INITIALIZED) {
        pach_memory_watch_reset(&g_probe_watch);

        if (result < 0) {
            return result;
        }

        return PACH_MEMORY_WATCH_ERROR_ARGUMENT;
    }

    return PACH_MEMORY_OK;
}

static void pach_log_game_initialized(
    u32 probe_kernel_address,
    u32 probe_value)
{
    if (!g_logger_ready) {
        return;
    }

    pach_log_key_value("game id", g_game_context.game_id);
    pach_log_line("game context initialized");
    pach_log_line("memory mapper self-test succeeded");
    pach_log_hex32("probe RA address", PACH_MEMORY_PROBE_RA_ADDRESS);
    pach_log_hex32("probe kernel address", probe_kernel_address);
    pach_log_hex32("probe value", probe_value);
    pach_log_line("memory read self-test succeeded");
    pach_log_hex32("watch initial value", g_probe_watch.current_value);
    pach_log_line("memory watch initialized");
    pach_log_line("game identification succeeded");
}

static void pach_reset_executable_detection(void)
{
    g_executable_state = PACH_EXECUTABLE_WAITING;
    g_executable_candidate = 0;
    g_executable_stable_samples = 0;
}

static void pach_update_executable_state(void)
{
    u32 current_value;
    int result;

    if (!g_game_context.initialized ||
        !g_probe_watch.initialized ||
        g_executable_state == PACH_EXECUTABLE_READY) {

        return;
    }

    result = pach_memory_watch_sample(
        &g_probe_watch,
        &g_game_context.memory_map
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_line("memory watch sampling failed");
        }

        pach_memory_watch_reset(&g_probe_watch);
        pach_reset_executable_detection();
        return;
    }

    current_value = g_probe_watch.current_value;

    if (pach_is_loader_magic(current_value)) {
        if (g_executable_state != PACH_EXECUTABLE_WAITING ||
            g_executable_candidate != current_value) {

            if (g_logger_ready) {
                pach_log_hex32("loader marker", current_value);

                if (current_value == PACH_PSP_HEADER_MAGIC) {
                    pach_log_line("PSP module header detected");
                } else {
                    pach_log_line("ELF header detected");
                }
            }
        }

        g_executable_state = PACH_EXECUTABLE_WAITING;
        g_executable_candidate = current_value;
        g_executable_stable_samples = 0;
        return;
    }

    if (g_executable_state != PACH_EXECUTABLE_STABILIZING ||
        g_executable_candidate != current_value) {

        g_executable_state = PACH_EXECUTABLE_STABILIZING;
        g_executable_candidate = current_value;
        g_executable_stable_samples = 1;

        if (g_logger_ready) {
            pach_log_hex32("executable candidate", current_value);
            pach_log_line("waiting for executable stability");
        }

        return;
    }

    if (g_executable_stable_samples <
        PACH_EXECUTABLE_STABLE_SAMPLES) {

        ++g_executable_stable_samples;
    }

    if (g_executable_stable_samples <
        PACH_EXECUTABLE_STABLE_SAMPLES) {

        return;
    }

    result = pach_memory_watch_rebase(&g_probe_watch);

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_line("memory watch rebase failed");
        }

        return;
    }

    g_executable_state = PACH_EXECUTABLE_READY;

    if (g_logger_ready) {
        pach_log_hex32("stable executable value", g_executable_candidate);
        pach_log_hex32("stable sample count", g_executable_stable_samples);
        pach_log_line("game executable ready");
    }
}

static int pach_prepare_profile(void)
{
    u32 restored_count = 0;
    u32 migrated_count = 0;
    u32 dropped_count = 0;
    u32 old_ids[PACH_ACHIEVEMENT_MAX];
    u32 old_count = 0;
    u32 index;
    int result;
    int recovered_from_backup = 0;
    int primary_profile_error = 0;

    result = pach_profile_build_paths(
        g_game_context.game_id,
        &g_profile_paths
    );

    if (result < 0) {
        return result;
    }

    if (g_logger_ready) {
        pach_log_key_value("profile path", g_profile_paths.main_path);
    }

    result = pach_profile_load(
        &g_profile_paths,
        g_game_context.game_id,
        g_package_info.package_id,
        &g_profile
    );

    recovered_from_backup =
        g_profile.recovered_from_backup;

    primary_profile_error =
        g_profile.primary_error;

    if (recovered_from_backup && g_logger_ready) {
        pach_log_hex32(
            "primary profile error",
            (u32)primary_profile_error
        );

        pach_log_line("profile recovered from backup");
    }

    if (result == PACH_PROFILE_NOT_FOUND) {
        if (g_logger_ready) {
            pach_log_line("profile not found");
        }

        result = pach_profile_init_new(
            &g_profile,
            g_game_context.game_id,
            g_package_info.package_id
        );

        if (result < 0) {
            return result;
        }

        result = pach_profile_save_atomic(
            &g_profile_paths,
            &g_profile
        );

        if (result < 0) {
            return result;
        }

        if (g_logger_ready) {
            pach_log_line("new profile created");
        }
    } else if (result == PACH_PROFILE_PACKAGE_MISMATCH) {
        old_count = g_profile.unlocked_count;

        for (index = 0; index < old_count; ++index) {
            old_ids[index] = g_profile.unlocked_ids[index];
        }

        if (g_logger_ready) {
            pach_log_line("profile package mismatch");
            pach_log_hex32("old profile package id", g_profile.package_id);
        }

        result = pach_profile_init_new(
            &g_profile,
            g_game_context.game_id,
            g_package_info.package_id
        );

        if (result < 0) {
            return result;
        }

        g_profile.recovered_from_backup =
            recovered_from_backup;

        g_profile.primary_error =
            primary_profile_error;

        for (index = 0; index < old_count; ++index) {
            if (!pach_package_contains_achievement_id(
                    &g_package_info,
                    old_ids[index])) {

                ++dropped_count;
                continue;
            }

            result = pach_profile_add_unlocked(
                &g_profile,
                old_ids[index]
            );

            if (result == PACH_PROFILE_ADDED ||
                result == PACH_PROFILE_EXISTS) {

                ++migrated_count;
            } else if (result < 0) {
                return result;
            }
        }

        result = pach_profile_save_atomic(
            &g_profile_paths,
            &g_profile
        );

        if (result < 0) {
            return result;
        }

        if (g_logger_ready) {
            pach_log_hex32("migrated achievement count", migrated_count);
            pach_log_hex32("dropped achievement count", dropped_count);
            pach_log_line("profile migrated");
        }
    } else if (result < 0) {
        return result;
    } else {
        if (recovered_from_backup) {
            result = pach_profile_save_atomic(
                &g_profile_paths,
                &g_profile
            );

            if (result < 0) {
                return result;
            }

            if (g_logger_ready) {
                pach_log_line("primary profile repaired");
            }
        }

        if (g_logger_ready) {
            pach_log_line("profile loaded");
        }
    }

    result = pach_achievement_engine_restore_ids(
        &g_achievement_engine,
        g_profile.unlocked_ids,
        g_profile.unlocked_count,
        &restored_count
    );

    if (result < 0) {
        return result;
    }

    g_profile_ready = 1;

    if (g_logger_ready) {
        pach_log_hex32("profile package id", g_profile.package_id);
        pach_log_hex32("profile unlocked count", g_profile.unlocked_count);
        pach_log_hex32("restored achievement count", restored_count);
        pach_log_line("profile ready");
    }

    return PACH_PROFILE_OK;
}

static void pach_disable_loaded_game_data(void)
{
    g_profile_ready = 0;
    g_achievement_engine_failed = 1;

    pach_profile_reset(&g_profile);
    pach_achievement_engine_reset(&g_achievement_engine);
    pach_badge_reset(&g_badge_pack);
    pach_package_reset(&g_package_info);
}

static void pach_try_load_badge_pack(void)
{
    int result;

    pach_badge_reset(&g_badge_pack);
    g_badge_path[0] = '\0';

    result = pach_badge_build_path(
        g_game_context.game_id,
        g_badge_path,
        sizeof(g_badge_path)
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("badge path error", (u32)result);
        }

        return;
    }

    if (g_logger_ready) {
        pach_log_key_value("badge path", g_badge_path);
    }

    result = pach_badge_load(
        g_badge_path,
        g_game_context.game_id,
        &g_badge_pack
    );

    if (result == PACH_BADGE_NOT_FOUND) {
        if (g_logger_ready) {
            pach_log_line("badge pack not found");
        }

        return;
    }

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("badge pack error", (u32)result);
            pach_log_line("badge pack loading failed");
        }

        pach_badge_reset(&g_badge_pack);
        return;
    }

    if (g_logger_ready) {
        pach_log_line("badge pack found");
        pach_log_hex32("badge format version", g_badge_pack.format_version);
        pach_log_key_value("badge game id", g_badge_pack.game_id);
        pach_log_hex32("badge count", g_badge_pack.badge_count);
        pach_log_hex32("badge pack id", g_badge_pack.pack_id);
        pach_log_line("badge pack loaded");
    }
}

static void pach_try_load_game_package(void)
{
    int result;

    if (g_package_attempted ||
        !g_game_context.initialized ||
        g_executable_state != PACH_EXECUTABLE_READY) {

        return;
    }

    g_package_attempted = 1;

    result = pach_package_build_path(
        g_game_context.game_id,
        g_package_path,
        sizeof(g_package_path)
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("package path error", (u32)result);
        }

        return;
    }

    if (g_logger_ready) {
        pach_log_key_value("package path", g_package_path);
    }

    result = pach_package_load(
        g_package_path,
        g_game_context.game_id,
        &g_package_info
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("package load error", (u32)result);
            pach_log_line("game package loading failed");
        }

        return;
    }

    result = pach_achievement_engine_init(
        &g_achievement_engine,
        g_package_info.achievements,
        g_package_info.achievement_count,
        g_package_info.groups,
        g_package_info.group_count,
        g_package_info.conditions,
        g_package_info.condition_count
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("achievement engine error", (u32)result);
            pach_log_line("achievement engine initialization failed");
        }

        pach_disable_loaded_game_data();
        return;
    }

    result = pach_achievement_engine_configure_scheduler(
        &g_achievement_engine,
        pach_config_scheduler_mode()
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32(
                "scheduler configuration error",
                (u32)result
            );
        }

        pach_disable_loaded_game_data();
        return;
    }

    result = pach_prepare_profile();

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("profile initialization error", (u32)result);
            pach_log_line("profile initialization failed");
        }

        pach_disable_loaded_game_data();
        return;
    }

    pach_try_load_badge_pack();

    if (g_logger_ready) {
        pach_log_line("game package found");
        pach_log_hex32("package format version", g_package_info.format_version);
        pach_log_key_value("package game id", g_package_info.game_id);
        pach_log_hex32("achievement count", g_package_info.achievement_count);
        pach_log_hex32("group count", g_package_info.group_count);
        pach_log_hex32("condition count", g_package_info.condition_count);
        pach_log_hex32("memory reference count", g_achievement_engine.memory_ref_count);
        pach_log_hex32(
            "sample cache capacity",
            PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX
        );

        if (g_achievement_engine.scheduler_enabled) {
            pach_log_line("adaptive achievement scheduler enabled");
            pach_log_hex32(
                "scheduler watch count",
                g_achievement_engine.scheduler_watch_count
            );
            pach_log_hex32(
                "scheduler peak conditions",
                g_achievement_engine.scheduler_peak_conditions
            );
        } else {
            pach_log_line("full-rate achievement scheduler");
        }
        pach_log_hex32("package flags", g_package_info.flags);
        pach_log_hex32("package id", g_package_info.package_id);
        pach_log_line("achievement engine initialized");
        pach_log_line("game package loaded");
    }

    if (pach_feedback_needed(1)) {
        result = pach_notification_enqueue_status(
            &g_notification_queue,
            (u16)g_package_info.achievement_count
        );

        if (result < 0) {
            if (g_logger_ready) {
                pach_log_hex32(
                    "startup notification queue error",
                    (u32)result
                );
            }
        } else if (g_logger_ready) {
            pach_log_line("startup notification queued");
        }
    }

    if (g_logger_ready) {
        pach_log_line("achievement menu ready");
    }
}

static void pach_tick_achievements(void)
{
    u32 event_count = 0;
    u32 index;

    int profile_changed = 0;
    int result;

    if (!g_package_info.loaded ||
        !g_achievement_engine.initialized ||
        !g_profile_ready ||
        g_overlay.menu_open ||
        g_achievement_engine_failed) {

        return;
    }

    result = pach_achievement_engine_tick(
        &g_achievement_engine,
        &g_game_context.memory_map,
        g_achievement_events,
        PACH_ACHIEVEMENT_MAX,
        &event_count
    );

    if (result < 0) {
        g_achievement_engine_failed = 1;

        if (g_logger_ready) {
            pach_log_hex32("achievement tick error", (u32)result);
            pach_log_line("achievement engine disabled");
        }

        return;
    }

    if (!g_performance_logged) {
        g_performance_logged = 1;

        if (g_logger_ready) {
            pach_log_hex32(
                "sample cache hits",
                g_achievement_engine.sample_cache_hits
            );

            pach_log_hex32(
                "sample cache misses",
                g_achievement_engine.sample_cache_misses
            );

            pach_log_line("performance sampling active");
        }
    }

    for (index = 0; index < event_count; ++index) {
        const PachAchievementEvent *event =
            &g_achievement_events[index];

        const PachAchievementDefinition *definition =
            &g_package_info.achievements[event->achievement_index];

        const char *title = pach_package_get_string(
            &g_package_info,
            definition->title_offset
        );

        if (title == NULL) {
            title = "Achievement";
        }

        if (g_logger_ready) {
            pach_log_line("achievement unlocked");
            pach_log_hex32("achievement id", event->achievement_id);
            pach_log_key_value("achievement title", title);
            pach_log_hex32("achievement points", definition->points);
        }

        if (pach_feedback_needed(0)) {
            result = pach_notification_enqueue(
                &g_notification_queue,
                event->achievement_id,
                definition->points,
                definition->type,
                title
            );

            if (result < 0) {
                if (g_logger_ready) {
                    pach_log_hex32(
                        "notification queue error",
                        (u32)result
                    );
                }
            } else if (g_logger_ready) {
                pach_log_line("notification queued");
            }
        }

        result = pach_profile_add_unlocked(
            &g_profile,
            event->achievement_id
        );

        if (result == PACH_PROFILE_ADDED) {
            profile_changed = 1;
        } else if (result < 0 && g_logger_ready) {
            pach_log_hex32("profile update error", (u32)result);
        }
    }

    if (!profile_changed) {
        return;
    }

    result = pach_profile_save_atomic(
        &g_profile_paths,
        &g_profile
    );

    if (result < 0) {
        if (g_logger_ready) {
            pach_log_hex32("profile save error", (u32)result);
            pach_log_line("profile save failed");
        }

        return;
    }

    if (g_logger_ready) {
        pach_log_hex32("saved unlocked count", g_profile.unlocked_count);
        pach_log_line("profile saved");
    }
}

static int pach_overlay_thread(
    SceSize args,
    void *argp)
{
    (void)args;
    (void)argp;

    return pach_overlay_run(&g_overlay);
}

static int pach_worker_thread(
    SceSize args,
    void *argp)
{
    char game_id[PACH_GAME_ID_CAPACITY];

    unsigned int tick_count = 0;
    unsigned int detection_attempts = 0;

    u32 probe_kernel_address = 0;
    u32 probe_value = 0;

    int game_detected = 0;
    int result;

    (void)args;
    (void)argp;

    pach_game_context_reset(&g_game_context);
    pach_memory_watch_reset(&g_probe_watch);
    pach_package_reset(&g_package_info);
    pach_badge_reset(&g_badge_pack);
    pach_profile_reset(&g_profile);
    pach_achievement_engine_reset(&g_achievement_engine);

    g_package_path[0] = '\0';
    g_badge_path[0] = '\0';
    g_profile_paths.main_path[0] = '\0';
    g_profile_paths.temp_path[0] = '\0';
    g_profile_paths.backup_path[0] = '\0';

    pach_reset_executable_detection();

    g_package_attempted = 0;
    g_achievement_engine_failed = 0;
    g_profile_ready = 0;
    g_performance_logged = 0;

    if (g_config.logging_enabled &&
        pach_log_init() >= 0) {

        g_logger_ready = 1;
        pach_log_line("worker thread started");
        pach_log_line("plugin mode: GAME");
        pach_log_configuration();
        pach_log_line("waiting for game identification");
    }

    while (g_running) {
        if (!game_detected &&
            detection_attempts < PACH_GAME_DETECT_MAX_TRIES) {

            ++detection_attempts;

            result = pach_game_detect(game_id);

            if (result >= 0) {
                result = pach_initialize_game(
                    game_id,
                    &probe_kernel_address,
                    &probe_value
                );

                if (result >= 0) {
                    result = pach_initialize_probe_watch();

                    if (result >= 0) {
                        game_detected = 1;

                        pach_log_game_initialized(
                            probe_kernel_address,
                            probe_value
                        );

                        pach_reset_executable_detection();
                    } else {
                        pach_memory_watch_reset(&g_probe_watch);
                        pach_game_context_reset(&g_game_context);

                        if (g_logger_ready) {
                            pach_log_line("memory watch initialization failed");
                        }
                    }
                } else if (g_logger_ready) {
                    pach_log_line("game context initialization failed");
                }
            }

            if (!game_detected &&
                detection_attempts == PACH_GAME_DETECT_MAX_TRIES &&
                g_logger_ready) {

                pach_log_line("game identification failed");
            }
        }

        if (game_detected) {
            pach_update_executable_state();
            pach_try_load_game_package();
            pach_tick_achievements();
        }

        if (g_achievement_engine.initialized &&
            g_profile_ready &&
            !g_achievement_engine_failed) {

            sceKernelDelayThread(PACH_ENGINE_TICK_INTERVAL_US);
        } else {
            sceKernelDelayThread(PACH_IDLE_TICK_INTERVAL_US);
        }

        ++tick_count;

        if (tick_count == PACH_ALIVE_TICK && g_logger_ready) {
            pach_log_line("worker thread is alive");
        }
    }

    pach_achievement_engine_reset(&g_achievement_engine);
    pach_profile_reset(&g_profile);
    pach_badge_reset(&g_badge_pack);
    pach_package_reset(&g_package_info);
    pach_memory_watch_reset(&g_probe_watch);
    pach_game_context_reset(&g_game_context);
    pach_reset_executable_detection();

    g_package_attempted = 0;
    g_achievement_engine_failed = 0;
    g_profile_ready = 0;
    g_performance_logged = 0;

    if (g_logger_ready) {
        pach_log_line("worker thread stopped");
    }

    return 0;
}

static int pach_stop_thread(
    SceUID *thread_id,
    int release_wait,
    const char *thread_name)
{
    SceUInt timeout = PACH_THREAD_STOP_TIMEOUT_US;
    SceUID id;
    int result;

    if (thread_id == NULL || *thread_id < 0) {
        return 0;
    }

    id = *thread_id;
    *thread_id = -1;

    if (release_wait) {
        (void)sceKernelReleaseWaitThread(id);
    }

    result = sceKernelWaitThreadEnd(id, &timeout);

    if (result < 0) {
        if (g_logger_ready && thread_name != NULL) {
            pach_log_key_value("forcing thread stop", thread_name);
        }

        result = sceKernelTerminateDeleteThread(id);

        if (result < 0) {
            result = sceKernelDeleteThread(id);
        }

        return result;
    }

    return sceKernelDeleteThread(id);
}

int module_start(SceSize args, void *argp)
{
    int result;

    (void)args;
    (void)argp;

    if (g_running || g_module_active) {
        return 0;
    }

    pach_config_reset(&g_config);
    g_config_result = pach_config_load(
        PACH_CONFIG_PATH,
        &g_config
    );

    if (!g_config.enabled) {
        return 0;
    }

    pach_notification_queue_reset(
        &g_notification_queue
    );

    pach_overlay_reset(
        &g_overlay
    );

    result = pach_notification_queue_init(
        &g_notification_queue
    );

    if (result < 0) {
        return result;
    }

    g_running = 1;

    result = pach_overlay_init(
        &g_overlay,
        &g_notification_queue,
        &g_running,
        &g_config,
        &g_package_info,
        &g_badge_pack,
        &g_achievement_engine,
        &g_profile,
        &g_profile_ready
    );

    if (result < 0) {
        g_running = 0;
        pach_notification_queue_shutdown(
            &g_notification_queue
        );
        return result;
    }

    g_overlay_thread_id = sceKernelCreateThread(
        "PachOverlay",
        pach_overlay_thread,
        0x19,
        0x4000,
        0,
        NULL
    );

    if (g_overlay_thread_id < 0) {
        result = g_overlay_thread_id;
        g_overlay_thread_id = -1;
        g_running = 0;
        pach_overlay_reset(&g_overlay);
        pach_notification_queue_shutdown(
            &g_notification_queue
        );
        return result;
    }

    result = sceKernelStartThread(
        g_overlay_thread_id,
        0,
        NULL
    );

    if (result < 0) {
        sceKernelDeleteThread(
            g_overlay_thread_id
        );

        g_overlay_thread_id = -1;
        g_running = 0;
        pach_overlay_reset(&g_overlay);
        pach_notification_queue_shutdown(
            &g_notification_queue
        );
        return result;
    }

    g_worker_thread_id = sceKernelCreateThread(
        "PachWorker",
        pach_worker_thread,
        0x38,
        0x4000,
        0,
        NULL
    );

    if (g_worker_thread_id < 0) {
        result = g_worker_thread_id;
        g_worker_thread_id = -1;
        g_running = 0;

        (void)pach_stop_thread(
            &g_overlay_thread_id,
            1,
            "overlay"
        );

        pach_overlay_reset(&g_overlay);
        pach_notification_queue_shutdown(
            &g_notification_queue
        );
        return result;
    }

    result = sceKernelStartThread(
        g_worker_thread_id,
        0,
        NULL
    );

    if (result < 0) {
        sceKernelDeleteThread(
            g_worker_thread_id
        );

        g_worker_thread_id = -1;
        g_running = 0;

        (void)pach_stop_thread(
            &g_overlay_thread_id,
            1,
            "overlay"
        );

        pach_overlay_reset(&g_overlay);
        pach_notification_queue_shutdown(
            &g_notification_queue
        );
        return result;
    }

    g_module_active = 1;
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    if (!g_module_active) {
        return 0;
    }

    if (g_logger_ready) {
        pach_log_line("module_stop requested");
    }

    g_running = 0;

    /*
     * The menu freezes the game's user threads to keep its framebuffer
     * stable. Resume them before waiting for plugin threads so HOME -> Quit
     * can never be blocked by an open achievement menu.
     */
    pach_overlay_prepare_stop(&g_overlay);

    /*
     * The display subsystem can stop producing VBlank events while a game
     * is being torn down. Release the overlay from its display wait and use
     * a bounded join so unloading the PRX can never block indefinitely.
     */
    (void)pach_stop_thread(
        &g_overlay_thread_id,
        1,
        "overlay"
    );

    (void)pach_stop_thread(
        &g_worker_thread_id,
        1,
        "worker"
    );

    pach_overlay_reset(&g_overlay);

    pach_notification_queue_shutdown(
        &g_notification_queue
    );

    if (g_logger_ready) {
        pach_log_line("module_stop complete");
    }

    g_logger_ready = 0;
    g_module_active = 0;
    return 0;
}
