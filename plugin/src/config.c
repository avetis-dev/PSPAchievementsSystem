#include "pach_config.h"

#include <stddef.h>

#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>

#define PACH_CONFIG_BUFFER_CAPACITY 4096u
#define PACH_CONFIG_TOKEN_CAPACITY    32u

typedef enum PachConfigSection {
    PACH_CONFIG_SECTION_NONE = 0,
    PACH_CONFIG_SECTION_GENERAL,
    PACH_CONFIG_SECTION_NOTIFICATIONS,
    PACH_CONFIG_SECTION_AUDIO,
    PACH_CONFIG_SECTION_MENU,
    PACH_CONFIG_SECTION_PERFORMANCE
} PachConfigSection;

static char g_pach_config_buffer[
    PACH_CONFIG_BUFFER_CAPACITY
];

static int pach_config_is_space(char value)
{
    return
        value == ' ' ||
        value == '\t' ||
        value == '\r' ||
        value == '\n';
}

static char pach_config_lower(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return (char)(value + ('a' - 'A'));
    }

    return value;
}

static int pach_config_text_equal(
    const char *left,
    const char *right)
{
    u32 index = 0;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (left[index] != '\0' &&
           right[index] != '\0') {

        if (pach_config_lower(left[index]) !=
            pach_config_lower(right[index])) {

            return 0;
        }

        ++index;
    }

    return
        left[index] == '\0' &&
        right[index] == '\0';
}

static char *pach_config_trim(char *text)
{
    char *start;
    char *end;

    if (text == NULL) {
        return NULL;
    }

    start = text;

    while (*start != '\0' &&
           pach_config_is_space(*start)) {

        ++start;
    }

    end = start;

    while (*end != '\0') {
        ++end;
    }

    while (end > start &&
           pach_config_is_space(end[-1])) {

        --end;
    }

    *end = '\0';

    return start;
}

static int pach_config_parse_u32(
    const char *text,
    u32 minimum,
    u32 maximum,
    u32 *value)
{
    u32 parsed = 0;
    u32 index = 0;

    if (text == NULL || value == NULL ||
        text[0] == '\0') {

        return 0;
    }

    while (text[index] != '\0') {
        u32 digit;

        if (text[index] < '0' ||
            text[index] > '9') {

            return 0;
        }

        digit = (u32)(text[index] - '0');

        if (parsed >
            (0xFFFFFFFFu - digit) / 10u) {

            return 0;
        }

        parsed = parsed * 10u + digit;
        ++index;
    }

    if (parsed < minimum ||
        parsed > maximum) {

        return 0;
    }

    *value = parsed;
    return 1;
}

static int pach_config_parse_bool(
    const char *text,
    int *value)
{
    if (text == NULL || value == NULL) {
        return 0;
    }

    if (pach_config_text_equal(text, "1") ||
        pach_config_text_equal(text, "true") ||
        pach_config_text_equal(text, "yes") ||
        pach_config_text_equal(text, "on")) {

        *value = 1;
        return 1;
    }

    if (pach_config_text_equal(text, "0") ||
        pach_config_text_equal(text, "false") ||
        pach_config_text_equal(text, "no") ||
        pach_config_text_equal(text, "off")) {

        *value = 0;
        return 1;
    }

    return 0;
}

static int pach_config_button_from_token(
    const char *token,
    u32 *button)
{
    if (token == NULL || button == NULL) {
        return 0;
    }

    if (pach_config_text_equal(token, "select")) {
        *button = PSP_CTRL_SELECT;
    } else if (pach_config_text_equal(token, "start")) {
        *button = PSP_CTRL_START;
    } else if (pach_config_text_equal(token, "up")) {
        *button = PSP_CTRL_UP;
    } else if (pach_config_text_equal(token, "right")) {
        *button = PSP_CTRL_RIGHT;
    } else if (pach_config_text_equal(token, "down")) {
        *button = PSP_CTRL_DOWN;
    } else if (pach_config_text_equal(token, "left")) {
        *button = PSP_CTRL_LEFT;
    } else if (pach_config_text_equal(token, "l") ||
               pach_config_text_equal(token, "ltrigger")) {
        *button = PSP_CTRL_LTRIGGER;
    } else if (pach_config_text_equal(token, "r") ||
               pach_config_text_equal(token, "rtrigger")) {
        *button = PSP_CTRL_RTRIGGER;
    } else if (pach_config_text_equal(token, "triangle")) {
        *button = PSP_CTRL_TRIANGLE;
    } else if (pach_config_text_equal(token, "circle")) {
        *button = PSP_CTRL_CIRCLE;
    } else if (pach_config_text_equal(token, "cross") ||
               pach_config_text_equal(token, "x")) {
        *button = PSP_CTRL_CROSS;
    } else if (pach_config_text_equal(token, "square")) {
        *button = PSP_CTRL_SQUARE;
    } else {
        return 0;
    }

    return 1;
}

static int pach_config_parse_buttons(
    const char *text,
    u32 *buttons)
{
    char token[PACH_CONFIG_TOKEN_CAPACITY];
    u32 result = 0;
    u32 token_length = 0;
    u32 index = 0;
    int token_count = 0;

    if (text == NULL || buttons == NULL) {
        return 0;
    }

    for (;;) {
        char value = text[index];

        if (value == '+' || value == '\0') {
            u32 button;
            char *trimmed;

            if (token_length >=
                PACH_CONFIG_TOKEN_CAPACITY) {

                return 0;
            }

            token[token_length] = '\0';
            trimmed = pach_config_trim(token);

            if (trimmed == NULL ||
                trimmed[0] == '\0' ||
                !pach_config_button_from_token(
                    trimmed,
                    &button)) {

                return 0;
            }

            result |= button;
            ++token_count;
            token_length = 0;

            if (value == '\0') {
                break;
            }
        } else {
            if (token_length + 1u >=
                PACH_CONFIG_TOKEN_CAPACITY) {

                return 0;
            }

            token[token_length] = value;
            ++token_length;
        }

        ++index;
    }

    if (token_count == 0 || result == 0) {
        return 0;
    }

    *buttons = result;
    return 1;
}

static PachConfigSection pach_config_parse_section(
    const char *text)
{
    if (pach_config_text_equal(text, "general")) {
        return PACH_CONFIG_SECTION_GENERAL;
    }

    if (pach_config_text_equal(text, "notifications")) {
        return PACH_CONFIG_SECTION_NOTIFICATIONS;
    }

    if (pach_config_text_equal(text, "audio")) {
        return PACH_CONFIG_SECTION_AUDIO;
    }

    if (pach_config_text_equal(text, "menu")) {
        return PACH_CONFIG_SECTION_MENU;
    }

    if (pach_config_text_equal(text, "performance")) {
        return PACH_CONFIG_SECTION_PERFORMANCE;
    }

    return PACH_CONFIG_SECTION_NONE;
}

static int pach_config_apply_value(
    PachConfig *config,
    PachConfigSection section,
    const char *key,
    const char *value)
{
    int boolean_value;
    u32 number_value;

    if (config == NULL || key == NULL || value == NULL) {
        return 0;
    }

    switch (section) {
    case PACH_CONFIG_SECTION_GENERAL:
        if (pach_config_text_equal(key, "enabled")) {
            return pach_config_parse_bool(
                value,
                &config->enabled
            );
        }

        if (pach_config_text_equal(key, "logging")) {
            return pach_config_parse_bool(
                value,
                &config->logging_enabled
            );
        }
        break;

    case PACH_CONFIG_SECTION_NOTIFICATIONS:
        if (pach_config_text_equal(key, "enabled")) {
            return pach_config_parse_bool(
                value,
                &config->notifications_enabled
            );
        }

        if (pach_config_text_equal(key, "startup")) {
            return pach_config_parse_bool(
                value,
                &config->startup_notification_enabled
            );
        }

        if (pach_config_text_equal(key, "duration_ms")) {
            if (!pach_config_parse_u32(
                    value,
                    PACH_CONFIG_MIN_NOTIFICATION_MS,
                    PACH_CONFIG_MAX_NOTIFICATION_MS,
                    &number_value)) {

                return 0;
            }

            config->notification_duration_ms = number_value;
            return 1;
        }

        if (pach_config_text_equal(key, "startup_duration_ms")) {
            if (!pach_config_parse_u32(
                    value,
                    PACH_CONFIG_MIN_NOTIFICATION_MS,
                    PACH_CONFIG_MAX_NOTIFICATION_MS,
                    &number_value)) {

                return 0;
            }

            config->startup_notification_duration_ms = number_value;
            return 1;
        }
        break;

    case PACH_CONFIG_SECTION_AUDIO:
        if (pach_config_text_equal(key, "enabled")) {
            return pach_config_parse_bool(
                value,
                &config->audio_enabled
            );
        }

        if (pach_config_text_equal(key, "volume")) {
            if (!pach_config_parse_u32(
                    value,
                    0,
                    100,
                    &number_value)) {

                return 0;
            }

            config->audio_volume_percent = number_value;
            return 1;
        }

        if (pach_config_text_equal(key, "startup_sound")) {
            return pach_config_parse_bool(
                value,
                &config->startup_sound_enabled
            );
        }

        if (pach_config_text_equal(key, "unlock_sound")) {
            return pach_config_parse_bool(
                value,
                &config->unlock_sound_enabled
            );
        }
        break;

    case PACH_CONFIG_SECTION_MENU:
        if (pach_config_text_equal(key, "enabled")) {
            return pach_config_parse_bool(
                value,
                &config->menu_enabled
            );
        }

        if (pach_config_text_equal(key, "hotkey")) {
            return pach_config_parse_buttons(
                value,
                &config->menu_buttons
            );
        }

        if (pach_config_text_equal(key, "hold_ms")) {
            if (!pach_config_parse_u32(
                    value,
                    PACH_CONFIG_MIN_MENU_HOLD_MS,
                    PACH_CONFIG_MAX_MENU_HOLD_MS,
                    &number_value)) {

                return 0;
            }

            config->menu_hold_ms = number_value;
            return 1;
        }

        if (pach_config_text_equal(key, "show_badges")) {
            return pach_config_parse_bool(
                value,
                &config->show_badges
            );
        }
        break;

    case PACH_CONFIG_SECTION_PERFORMANCE:
        if (!pach_config_text_equal(key, "mode")) {
            break;
        }

        if (pach_config_text_equal(value, "auto")) {
            config->performance_mode = PACH_PERFORMANCE_AUTO;
            return 1;
        }

        if (pach_config_text_equal(value, "full")) {
            config->performance_mode = PACH_PERFORMANCE_FULL;
            return 1;
        }

        if (pach_config_text_equal(value, "adaptive")) {
            config->performance_mode = PACH_PERFORMANCE_ADAPTIVE;
            return 1;
        }
        break;

    default:
        break;
    }

    (void)boolean_value;
    return 0;
}

void pach_config_reset(PachConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->enabled = 1;
    config->logging_enabled = 1;

    config->notifications_enabled = 1;
    config->startup_notification_enabled = 1;
    config->notification_duration_ms =
        PACH_CONFIG_DEFAULT_NOTIFICATION_MS;
    config->startup_notification_duration_ms =
        PACH_CONFIG_DEFAULT_STARTUP_MS;

    config->audio_enabled = 1;
    config->audio_volume_percent = 100u;
    config->startup_sound_enabled = 1;
    config->unlock_sound_enabled = 1;

    config->menu_enabled = 1;
    config->menu_buttons = PACH_CONFIG_BUTTON_MASK_DEFAULT;
    config->menu_hold_ms = PACH_CONFIG_DEFAULT_MENU_HOLD_MS;
    config->show_badges = 1;

    config->performance_mode = PACH_PERFORMANCE_AUTO;
    config->loaded = 0;
    config->warning_count = 0;
}

int pach_config_load(
    const char *path,
    PachConfig *config)
{
    SceUID file;
    int bytes_read;
    u32 position = 0;
    PachConfigSection section = PACH_CONFIG_SECTION_NONE;

    if (path == NULL || config == NULL) {
        return PACH_CONFIG_ERROR_ARGUMENT;
    }

    pach_config_reset(config);

    file = sceIoOpen(path, PSP_O_RDONLY, 0777);

    if (file < 0) {
        return PACH_CONFIG_NOT_FOUND;
    }

    bytes_read = sceIoRead(
        file,
        g_pach_config_buffer,
        PACH_CONFIG_BUFFER_CAPACITY - 1u
    );

    sceIoClose(file);

    if (bytes_read < 0) {
        return PACH_CONFIG_ERROR_READ;
    }

    if ((u32)bytes_read >=
        PACH_CONFIG_BUFFER_CAPACITY - 1u) {

        return PACH_CONFIG_ERROR_SIZE;
    }

    g_pach_config_buffer[bytes_read] = '\0';

    while (position < (u32)bytes_read) {
        char *line = &g_pach_config_buffer[position];
        char *cursor = line;
        char *trimmed;
        char *comment;
        char *separator;

        while (position < (u32)bytes_read &&
               g_pach_config_buffer[position] != '\n') {

            ++position;
        }

        if (position < (u32)bytes_read) {
            g_pach_config_buffer[position] = '\0';
            ++position;
        }

        comment = cursor;

        while (*comment != '\0' &&
               *comment != '#' &&
               *comment != ';') {

            ++comment;
        }

        *comment = '\0';
        trimmed = pach_config_trim(cursor);

        if (trimmed == NULL || trimmed[0] == '\0') {
            continue;
        }

        if (trimmed[0] == '[') {
            char *end = trimmed + 1;

            while (*end != '\0' && *end != ']') {
                ++end;
            }

            if (*end != ']') {
                ++config->warning_count;
                section = PACH_CONFIG_SECTION_NONE;
                continue;
            }

            *end = '\0';
            section = pach_config_parse_section(
                pach_config_trim(trimmed + 1)
            );

            if (section == PACH_CONFIG_SECTION_NONE) {
                ++config->warning_count;
            }

            continue;
        }

        separator = trimmed;

        while (*separator != '\0' &&
               *separator != '=') {

            ++separator;
        }

        if (*separator != '=') {
            ++config->warning_count;
            continue;
        }

        *separator = '\0';

        if (!pach_config_apply_value(
                config,
                section,
                pach_config_trim(trimmed),
                pach_config_trim(separator + 1))) {

            ++config->warning_count;
        }
    }

    config->loaded = 1;
    return PACH_CONFIG_OK;
}

u32 pach_config_ms_to_frames(u32 milliseconds)
{
    u32 frames;

    if (milliseconds == 0) {
        return 1u;
    }

    frames =
        (milliseconds * 60u + 999u) /
        1000u;

    if (frames == 0) {
        frames = 1u;
    }

    return frames;
}

const char *pach_config_performance_mode_name(u8 mode)
{
    if (mode == PACH_PERFORMANCE_FULL) {
        return "full";
    }

    if (mode == PACH_PERFORMANCE_ADAPTIVE) {
        return "adaptive";
    }

    return "auto";
}
