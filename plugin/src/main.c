#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <string.h>

#include "paths.h"
#include "profile.h"
#include "game_db.h"
#include "game_map.h"
#include "detect.h"
#include "popup.h"
#include "rcheevos_glue.h"
#include "memory.h"

PSP_MODULE_INFO("PspAchievements", 0x1000, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define LOG_PATH "ms0:/PSP/ACH/pach_log.txt"
static void log_msg(const char *msg) {
    SceUID fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) { sceIoWrite(fd, msg, strlen(msg)); sceIoWrite(fd, "\n", 1); sceIoClose(fd); }
}
static void log_int(const char *prefix, int val) {
    char buf[128], num[16];
    int i = 0, pos = 0, n = val < 0 ? -val : val;
    if (n == 0) num[i++] = '0';
    else while (n > 0) { num[i++] = '0' + (n % 10); n /= 10; }
    while (*prefix && pos < 100) buf[pos++] = *prefix++;
    if (val < 0) buf[pos++] = '-';
    for (int j = i - 1; j >= 0; j--) buf[pos++] = num[j];
    buf[pos] = '\0';
    log_msg(buf);
}

static volatile int g_running = 1;
static SceUID g_logic_thid = -1;
static SceUID g_draw_thid = -1;
static PACH_ProfileData          g_profile;
static PACH_GameMapDb            g_mapdb;
static PACH_LoadedGame           g_game;
static PACH_ProfileGameProgress *g_game_progress = NULL;
static RC_RuntimeState           g_rc_state;
static RC_ParsedAchievement      g_parsed[PACH_MAX_GAME_ACH];
static int                       g_num_parsed = 0;
static int                       g_game_loaded = 0;
static char                      g_game_code[16];

/* ============================================================
 * DRAW THREAD — separate, low priority, synchronized with VSync
 * ============================================================ */
static int draw_thread_func(SceSize args, void *argp) {
    (void)args; (void)argp;
    while (g_running) {
        if (!pach_popup_is_active()) {
            sceKernelDelayThread(50 * 1000);
            continue;
        }
        /* Wait for VSync, then render — this ensures we draw at the correct screen refresh timing */
        sceDisplayWaitVblankStart();
        pach_popup_draw_current();
        /* Small delay and render again for the second buffer */
        sceKernelDelayThread(8000);
        pach_popup_draw_current();
    }
    sceKernelExitDeleteThread(0);
    return 0;
}

/* ============================================================
 * LOGIC THREAD
 * ============================================================ */
static int logic_thread_func(SceSize args, void *argp) {
    (void)args; (void)argp;
    int warmup_frames = 0;
    int debug_log_counter = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pach_popup_init();

    sceKernelDelayThread(5 * 1000 * 1000);

    if (g_game_loaded) {
        pach_popup_show("Achievements", "System Loaded OK");
        log_msg("System loaded, starting warmup");
    }

    while (g_running) {
        pach_popup_update();

        /* WARMUP: 200 frames (~10 seconds) only warm up delta */
        if (warmup_frames < 200) {
            warmup_frames++;
            if (g_game_loaded && g_num_parsed > 0) {
                rc_glue_update(&g_game, g_game_progress, &g_rc_state, g_parsed, g_num_parsed);
            }
            if (warmup_frames == 200) {
                log_msg("Warmup done");
                for (int i = 0; i < g_num_parsed; i++) {
                    for (int g = 0; g < g_parsed[i].num_groups; g++) {
                        for (int c = 0; c < g_parsed[i].groups[g].count; c++) {
                            g_parsed[i].groups[g].conds[c].current_hits = 0;
                        }
                    }
                }
            }
            sceKernelDelayThread(50 * 1000);
            continue;
        }

        /* Check achievements */
        if (g_game_loaded && g_game_progress && g_game.loaded && g_num_parsed > 0) {
            RC_EvalResult res = rc_glue_update(&g_game, g_game_progress, &g_rc_state, g_parsed, g_num_parsed);
            if (res.unlocked_index >= 0 && res.unlocked_def) {
                log_msg("ACHIEVEMENT UNLOCKED!");
                log_msg(res.unlocked_def->title);
                pach_popup_show(res.unlocked_def->title, res.unlocked_def->desc);
                pach_profile_save(&g_profile);
            }
        }

        /* DEBUG: every 600 frames (~30 sec) log key memory addresses
            to verify that game memory is being read correctly */
        debug_log_counter++;
        if (debug_log_counter >= 600) {
            debug_log_counter = 0;
            if (g_game_loaded) {
                /* Read key memory addresses of Silent Hill Origins */
                unsigned int map_id = pach_mem_read32(0x9fda18);   /* current map */
                unsigned int notes  = pach_mem_read8(0x9fdae8);    /* number of notes */
                unsigned int items  = pach_mem_read8(0x9fdab8);    /* inventory */
                
                log_msg("--- DEBUG MEMORY ---");
                log_int("map_id=", (int)map_id);
                log_int("notes=", (int)notes);
                log_int("items=", (int)items);
                log_int("delta_slots=", g_rc_state.num_slots);
                log_int("parsed=", g_num_parsed);
            }
        }

        /* L+R = debug */
        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(&pad, 1);
        if ((pad.Buttons & PSP_CTRL_LTRIGGER) && (pad.Buttons & PSP_CTRL_RTRIGGER)) {
            pach_popup_show("Debug", "Engine ACTIVE");
            sceKernelDelayThread(500 * 1000);
        }

        sceKernelDelayThread(50 * 1000);
    }
    sceKernelExitDeleteThread(0);
    return 0;
}

int module_start(SceSize args, void *argp) {
    (void)args; (void)argp;
    g_running = 1;

    sceIoMkdir("ms0:/PSP/ACH", 0777);
    sceIoMkdir("ms0:/PSP/ACH/games", 0777);
    sceIoMkdir("ms0:/PSP/ACH/profiles", 0777);
    
    SceUID fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) { sceIoWrite(fd, "=== PLUGIN START ===\n", 21); sceIoClose(fd); }

    memset(&g_profile, 0, sizeof(g_profile));
    pach_profile_init_empty(&g_profile, "default");
    
    if (pach_detect_game_code(g_game_code, sizeof(g_game_code))) {
        log_msg(g_game_code);
        if (pach_gamemap_load(&g_mapdb, PACH_GAME_MAP_FILE)) {
            PACH_GameMapEntry *entry = pach_gamemap_find_by_code(&g_mapdb, g_game_code);
            if (entry) {
                char path[128];
                strcpy(path, PACH_GAMES_DIR);
                strcat(path, entry->ach_file);
                if (pach_game_load_file(&g_game, path)) {
                    g_game_progress = pach_profile_get_or_create_game(&g_profile, g_game.header.game_id, g_game.header.num_achievements);
                    rc_glue_init(&g_rc_state);
                    g_num_parsed = rc_glue_parse_all(&g_game, g_parsed, PACH_MAX_GAME_ACH);
                    g_game_loaded = 1;
                    log_msg("Game loaded OK");
                    log_int("Parsed achs=", g_num_parsed);
                }
            }
        }
    }

    /* Logic thread — medium priority */
    g_logic_thid = sceKernelCreateThread("pach_logic", logic_thread_func, 0x30, 0x8000, 0, 0);
    if (g_logic_thid >= 0) sceKernelStartThread(g_logic_thid, 0, 0);

    /* Draw thread — low priority (0x40), to not interfere with the game */
    g_draw_thid = sceKernelCreateThread("pach_draw", draw_thread_func, 0x40, 0x4000, 0, 0);
    if (g_draw_thid >= 0) sceKernelStartThread(g_draw_thid, 0, 0);

    return 0;
}

int module_stop(SceSize args, void *argp) {
    (void)args; (void)argp;
    g_running = 0;
    if (g_logic_thid >= 0) { sceKernelWaitThreadEnd(g_logic_thid, 0); sceKernelDeleteThread(g_logic_thid); }
    if (g_draw_thid >= 0) { sceKernelWaitThreadEnd(g_draw_thid, 0); sceKernelDeleteThread(g_draw_thid); }
    if (g_game_loaded) pach_profile_save(&g_profile);
    return 0;
}