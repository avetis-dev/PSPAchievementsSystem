#include "pach_overlay.h"

#include <stddef.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspthreadman.h>

#define PACH_SCREEN_MIN_WIDTH  160
#define PACH_SCREEN_MIN_HEIGHT 80

#define PACH_PANEL_WIDTH  296
#define PACH_PANEL_HEIGHT 58
#define PACH_PANEL_MARGIN 8

#define PACH_CONTENT_LEFT    50
#define PACH_CONTENT_RIGHT   9

#define PACH_FONT_WIDTH   5
#define PACH_FONT_HEIGHT  7
#define PACH_FONT_ADVANCE 6

#define PACH_TITLE_LINE_CAPACITY 80

/* PSP CPU uncached alias for direct framebuffer access. */
#define PACH_UNCACHED_ALIAS_BIT 0x40000000u

#define PACH_MENU_MARGIN_X       8
#define PACH_MENU_MARGIN_Y       6
#define PACH_MENU_ROW_HEIGHT    19
#define PACH_MENU_LIST_OFFSET_Y 64
#define PACH_MENU_DETAIL_Y     207
#define PACH_MENU_FOOTER_Y     247

#define PACH_THREAD_CLASS_MASK 0xE0000000u

typedef struct PachColor {
    u8 red;
    u8 green;
    u8 blue;
} PachColor;

typedef struct PachOverlaySurface {
    void *pixels;
    int buffer_width;
    int pixel_format;
    int screen_width;
    int screen_height;
} PachOverlaySurface;

/* Compact 5x7 font. Lowercase input is converted to uppercase. */
static const u8 g_pach_font[95][PACH_FONT_HEIGHT] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!' - 32] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
    ['\'' - 32] = {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00},
    ['(' - 32] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    [')' - 32] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    ['+' - 32] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    [',' - 32] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08},
    ['-' - 32] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    ['.' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06},
    ['/' - 32] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},
    [':' - 32] = {0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00},
    ['?' - 32] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},

    ['0' - 32] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    ['1' - 32] = {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F},
    ['2' - 32] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    ['3' - 32] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    ['4' - 32] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    ['5' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    ['6' - 32] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    ['7' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    ['8' - 32] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    ['9' - 32] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},

    ['A' - 32] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['B' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    ['C' - 32] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    ['D' - 32] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    ['E' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    ['F' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    ['G' - 32] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    ['H' - 32] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['I' - 32] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['J' - 32] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
    ['K' - 32] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    ['L' - 32] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    ['M' - 32] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    ['N' - 32] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    ['O' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['P' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    ['Q' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    ['R' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    ['S' - 32] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    ['T' - 32] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    ['U' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['V' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    ['W' - 32] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    ['X' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    ['Y' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    ['Z' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
};

static const PachColor PACH_COLOR_PANEL = {12, 16, 23};
static const PachColor PACH_COLOR_PANEL_EDGE = {64, 73, 86};
static const PachColor PACH_COLOR_TEXT = {247, 249, 252};
static const PachColor PACH_COLOR_SUBTEXT = {186, 198, 212};
static const PachColor PACH_COLOR_ICON_BG = {22, 28, 38};
static const PachColor PACH_COLOR_MENU_BG = {7, 10, 15};
static const PachColor PACH_COLOR_MENU_HEADER = {15, 22, 32};
static const PachColor PACH_COLOR_MENU_SELECTED = {30, 43, 59};
static const PachColor PACH_COLOR_MENU_LOCKED = {132, 145, 160};
static const PachColor PACH_COLOR_MENU_TRACK = {27, 36, 48};
static const PachColor PACH_COLOR_MENU_GREEN = {64, 211, 153};

static void pach_overlay_clear_notification(
    PachNotification *notification)
{
    u32 index;

    if (notification == NULL) {
        return;
    }

    notification->achievement_id = 0;
    notification->points = 0;
    notification->type = 0;
    notification->kind = PACH_NOTIFICATION_KIND_ACHIEVEMENT;

    for (index = 0;
         index < PACH_ACHIEVEMENT_TITLE_CAPACITY;
         ++index) {

        notification->title[index] = '\0';
    }
}

static char pach_overlay_uppercase(char value)
{
    if (value >= 'a' && value <= 'z') {
        return (char)(value - ('a' - 'A'));
    }

    return value;
}

static const u8 *pach_overlay_get_glyph(char value)
{
    const u8 *glyph;
    int row;

    value = pach_overlay_uppercase(value);

    if (value < 32 || value > 126) {
        value = '?';
    }

    glyph = g_pach_font[(int)value - 32];

    if (value == ' ') {
        return glyph;
    }

    for (row = 0; row < PACH_FONT_HEIGHT; ++row) {
        if (glyph[row] != 0) {
            return glyph;
        }
    }

    return g_pach_font['?' - 32];
}

static void *pach_overlay_uncached_pointer(void *pointer)
{
    u32 address;

    if (pointer == NULL) {
        return NULL;
    }

    address = (u32)pointer;
    address |= PACH_UNCACHED_ALIAS_BIT;

    return (void *)address;
}

static u32 pach_overlay_pack_8888(PachColor color)
{
    return
        (u32)color.red |
        ((u32)color.green << 8) |
        ((u32)color.blue << 16) |
        0xFF000000u;
}

static u16 pach_overlay_pack_565(PachColor color)
{
    return
        (u16)(color.red >> 3) |
        (u16)((u16)(color.green >> 2) << 5) |
        (u16)((u16)(color.blue >> 3) << 11);
}

static u16 pach_overlay_pack_5551(PachColor color)
{
    return
        (u16)(color.red >> 3) |
        (u16)((u16)(color.green >> 3) << 5) |
        (u16)((u16)(color.blue >> 3) << 10) |
        0x8000u;
}

static u16 pach_overlay_pack_4444(PachColor color)
{
    return
        (u16)(color.red >> 4) |
        (u16)((u16)(color.green >> 4) << 4) |
        (u16)((u16)(color.blue >> 4) << 8) |
        0xF000u;
}

static int pach_overlay_surface_valid(
    const PachOverlaySurface *surface)
{
    if (surface == NULL ||
        surface->pixels == NULL ||
        surface->screen_width < PACH_SCREEN_MIN_WIDTH ||
        surface->screen_height < PACH_SCREEN_MIN_HEIGHT ||
        surface->buffer_width < surface->screen_width) {

        return 0;
    }

    return
        surface->pixel_format == PSP_DISPLAY_PIXEL_FORMAT_565 ||
        surface->pixel_format == PSP_DISPLAY_PIXEL_FORMAT_5551 ||
        surface->pixel_format == PSP_DISPLAY_PIXEL_FORMAT_4444 ||
        surface->pixel_format == PSP_DISPLAY_PIXEL_FORMAT_8888;
}

static int pach_overlay_get_surface(
    PachOverlaySurface *surface)
{
    int mode = 0;
    int result;

    if (surface == NULL) {
        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    surface->pixels = NULL;
    surface->buffer_width = 0;
    surface->pixel_format = 0;
    surface->screen_width = 0;
    surface->screen_height = 0;

    result = sceDisplayGetMode(
        &mode,
        &surface->screen_width,
        &surface->screen_height
    );

    if (result < 0) {
        return PACH_OVERLAY_ERROR_DISPLAY;
    }

    result = sceDisplayGetFrameBuf(
        &surface->pixels,
        &surface->buffer_width,
        &surface->pixel_format,
        PSP_DISPLAY_SETBUF_IMMEDIATE
    );

    if (result < 0) {
        return PACH_OVERLAY_ERROR_DISPLAY;
    }

    surface->pixels = pach_overlay_uncached_pointer(
        surface->pixels
    );

    if (!pach_overlay_surface_valid(surface)) {
        return PACH_OVERLAY_ERROR_FORMAT;
    }

    return PACH_OVERLAY_OK;
}

static void pach_overlay_put_pixel(
    const PachOverlaySurface *surface,
    int x,
    int y,
    PachColor color)
{
    u32 offset;

    if (!pach_overlay_surface_valid(surface) ||
        x < 0 || y < 0 ||
        x >= surface->screen_width ||
        y >= surface->screen_height) {

        return;
    }

    offset =
        (u32)y * (u32)surface->buffer_width +
        (u32)x;

    if (surface->pixel_format ==
        PSP_DISPLAY_PIXEL_FORMAT_8888) {

        volatile u32 *pixels =
            (volatile u32 *)surface->pixels;

        pixels[offset] = pach_overlay_pack_8888(color);
        return;
    }

    {
        volatile u16 *pixels =
            (volatile u16 *)surface->pixels;

        if (surface->pixel_format ==
            PSP_DISPLAY_PIXEL_FORMAT_565) {

            pixels[offset] = pach_overlay_pack_565(color);
        } else if (surface->pixel_format ==
            PSP_DISPLAY_PIXEL_FORMAT_5551) {

            pixels[offset] = pach_overlay_pack_5551(color);
        } else {
            pixels[offset] = pach_overlay_pack_4444(color);
        }
    }
}

static void pach_overlay_fill_rect(
    const PachOverlaySurface *surface,
    int x,
    int y,
    int width,
    int height,
    PachColor color)
{
    int clipped_x0;
    int clipped_y0;
    int clipped_x1;
    int clipped_y1;
    int current_x;
    int current_y;

    if (!pach_overlay_surface_valid(surface) ||
        width <= 0 || height <= 0) {

        return;
    }

    clipped_x0 = x < 0 ? 0 : x;
    clipped_y0 = y < 0 ? 0 : y;
    clipped_x1 = x + width;
    clipped_y1 = y + height;

    if (clipped_x1 > surface->screen_width) {
        clipped_x1 = surface->screen_width;
    }

    if (clipped_y1 > surface->screen_height) {
        clipped_y1 = surface->screen_height;
    }

    if (clipped_x0 >= clipped_x1 ||
        clipped_y0 >= clipped_y1) {

        return;
    }

    if (surface->pixel_format ==
        PSP_DISPLAY_PIXEL_FORMAT_8888) {

        volatile u32 *pixels =
            (volatile u32 *)surface->pixels;

        u32 packed = pach_overlay_pack_8888(color);

        for (current_y = clipped_y0;
             current_y < clipped_y1;
             ++current_y) {

            volatile u32 *row =
                pixels +
                current_y * surface->buffer_width +
                clipped_x0;

            for (current_x = clipped_x0;
                 current_x < clipped_x1;
                 ++current_x) {

                *row++ = packed;
            }
        }

        return;
    }

    {
        volatile u16 *pixels =
            (volatile u16 *)surface->pixels;

        u16 packed;

        if (surface->pixel_format ==
            PSP_DISPLAY_PIXEL_FORMAT_565) {

            packed = pach_overlay_pack_565(color);
        } else if (surface->pixel_format ==
            PSP_DISPLAY_PIXEL_FORMAT_5551) {

            packed = pach_overlay_pack_5551(color);
        } else {
            packed = pach_overlay_pack_4444(color);
        }

        for (current_y = clipped_y0;
             current_y < clipped_y1;
             ++current_y) {

            volatile u16 *row =
                pixels +
                current_y * surface->buffer_width +
                clipped_x0;

            for (current_x = clipped_x0;
                 current_x < clipped_x1;
                 ++current_x) {

                *row++ = packed;
            }
        }
    }
}

static PachColor pach_overlay_unpack_565(
    u16 value)
{
    PachColor color;
    u8 red5 = (u8)(value & 0x1Fu);
    u8 green6 = (u8)((value >> 5) & 0x3Fu);
    u8 blue5 = (u8)((value >> 11) & 0x1Fu);

    color.red = (u8)((red5 << 3) | (red5 >> 2));
    color.green = (u8)((green6 << 2) | (green6 >> 4));
    color.blue = (u8)((blue5 << 3) | (blue5 >> 2));

    return color;
}

static PachColor pach_overlay_dim_badge_color(
    PachColor color)
{
    u32 luminance =
        (u32)color.red * 3u +
        (u32)color.green * 6u +
        (u32)color.blue;

    u8 dimmed;

    luminance /= 10u;
    dimmed = (u8)(luminance * 2u / 5u);

    color.red = dimmed;
    color.green = dimmed;
    color.blue = dimmed;

    return color;
}

static void pach_overlay_draw_badge(
    const PachOverlaySurface *surface,
    int x,
    int y,
    int size,
    const u16 *pixels,
    int locked,
    PachColor accent)
{
    PachColor border;
    int target_x;
    int target_y;

    if (!pach_overlay_surface_valid(surface) ||
        pixels == NULL ||
        size <= 0) {

        return;
    }

    border = locked
        ? PACH_COLOR_MENU_LOCKED
        : accent;

    pach_overlay_fill_rect(
        surface,
        x,
        y,
        size + 2,
        size + 2,
        border
    );

    for (target_y = 0;
         target_y < size;
         ++target_y) {

        int source_y =
            target_y * (int)PACH_BADGE_HEIGHT /
            size;

        for (target_x = 0;
             target_x < size;
             ++target_x) {

            int source_x =
                target_x * (int)PACH_BADGE_WIDTH /
                size;

            u16 source_pixel =
                pixels[
                    source_y * (int)PACH_BADGE_WIDTH +
                    source_x
                ];

            PachColor color =
                pach_overlay_unpack_565(
                    source_pixel
                );

            if (locked) {
                color = pach_overlay_dim_badge_color(
                    color
                );
            }

            pach_overlay_put_pixel(
                surface,
                x + 1 + target_x,
                y + 1 + target_y,
                color
            );
        }
    }
}

static void pach_overlay_draw_char(
    const PachOverlaySurface *surface,
    int x,
    int y,
    char value,
    PachColor color)
{
    const u8 *glyph = pach_overlay_get_glyph(value);
    int row;
    int column;

    for (row = 0; row < PACH_FONT_HEIGHT; ++row) {
        for (column = 0; column < PACH_FONT_WIDTH; ++column) {
            u8 mask = (u8)(1u <<
                (PACH_FONT_WIDTH - 1 - column));

            if ((glyph[row] & mask) != 0) {
                pach_overlay_put_pixel(
                    surface,
                    x + column,
                    y + row,
                    color
                );
            }
        }
    }
}

static void pach_overlay_draw_text(
    const PachOverlaySurface *surface,
    int x,
    int y,
    const char *text,
    int max_characters,
    PachColor color)
{
    int index = 0;

    if (text == NULL || max_characters <= 0) {
        return;
    }

    while (text[index] != '\0' &&
           index < max_characters) {

        pach_overlay_draw_char(
            surface,
            x + index * PACH_FONT_ADVANCE,
            y,
            text[index],
            color
        );

        ++index;
    }
}

static int pach_overlay_string_length(const char *text)
{
    int length = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static void pach_overlay_copy_range(
    char output[PACH_TITLE_LINE_CAPACITY],
    const char *source,
    int start,
    int length)
{
    int index;

    if (output == NULL || source == NULL) {
        return;
    }

    if (length > PACH_TITLE_LINE_CAPACITY - 1) {
        length = PACH_TITLE_LINE_CAPACITY - 1;
    }

    for (index = 0; index < length; ++index) {
        output[index] = source[start + index];
    }

    output[length] = '\0';
}

static void pach_overlay_wrap_title(
    const char *title,
    int max_characters,
    char line_one[PACH_TITLE_LINE_CAPACITY],
    char line_two[PACH_TITLE_LINE_CAPACITY])
{
    int length;
    int split;
    int second_start;
    int second_length;

    line_one[0] = '\0';
    line_two[0] = '\0';

    if (title == NULL || max_characters <= 0) {
        return;
    }

    if (max_characters > PACH_TITLE_LINE_CAPACITY - 1) {
        max_characters = PACH_TITLE_LINE_CAPACITY - 1;
    }

    length = pach_overlay_string_length(title);

    if (length <= max_characters) {
        pach_overlay_copy_range(line_one, title, 0, length);
        return;
    }

    split = max_characters;

    while (split > 0 && title[split] != ' ') {
        --split;
    }

    if (split < max_characters / 2) {
        split = max_characters;
    }

    pach_overlay_copy_range(line_one, title, 0, split);

    second_start = split;

    while (title[second_start] == ' ') {
        ++second_start;
    }

    second_length = length - second_start;

    if (second_length <= max_characters) {
        pach_overlay_copy_range(
            line_two,
            title,
            second_start,
            second_length
        );
        return;
    }

    pach_overlay_copy_range(
        line_two,
        title,
        second_start,
        max_characters
    );

    if (max_characters >= 3) {
        line_two[max_characters - 3] = '.';
        line_two[max_characters - 2] = '.';
        line_two[max_characters - 1] = '.';
        line_two[max_characters] = '\0';
    }
}

static void pach_overlay_append_number(
    char *output,
    int capacity,
    u32 value,
    const char *suffix)
{
    char digits[10];
    int digit_count = 0;
    int position = 0;
    int index;

    if (output == NULL || capacity <= 0) {
        return;
    }

    do {
        digits[digit_count++] =
            (char)('0' + (value % 10u));
        value /= 10u;
    } while (value > 0 && digit_count < 10);

    for (index = digit_count - 1;
         index >= 0 && position + 1 < capacity;
         --index) {

        output[position++] = digits[index];
    }

    if (suffix != NULL) {
        index = 0;

        while (suffix[index] != '\0' &&
               position + 1 < capacity) {

            output[position++] = suffix[index++];
        }
    }

    output[position] = '\0';
}

static void pach_overlay_build_value_text(
    const PachNotification *notification,
    char output[20])
{
    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        pach_overlay_append_number(
            output,
            20,
            notification->points,
            " READY"
        );
        return;
    }

    output[0] = '+';

    pach_overlay_append_number(
        &output[1],
        19,
        notification->points,
        " PTS"
    );
}

static PachColor pach_overlay_accent_color(
    const PachNotification *notification)
{
    PachColor color;

    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        color.red = 64;
        color.green = 211;
        color.blue = 153;
        return color;
    }

    if (notification->type == 1) {
        color.red = 50;
        color.green = 205;
        color.blue = 183;
        return color;
    }

    if (notification->type == 2) {
        color.red = 245;
        color.green = 158;
        color.blue = 54;
        return color;
    }

    if (notification->type == 3) {
        color.red = 247;
        color.green = 205;
        color.blue = 70;
        return color;
    }

    color.red = 91;
    color.green = 157;
    color.blue = 255;
    return color;
}

static const char *pach_overlay_label(
    const PachNotification *notification)
{
    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        return "PSPACHIEVEMENTSNG";
    }

    if (notification->type == 1) {
        return "STORY ACHIEVEMENT";
    }

    if (notification->type == 2) {
        return "MISSABLE ACHIEVEMENT";
    }

    if (notification->type == 3) {
        return "GAME COMPLETED";
    }

    return "ACHIEVEMENT UNLOCKED";
}

static void pach_overlay_draw_trophy(
    const PachOverlaySurface *surface,
    int x,
    int y,
    PachColor accent)
{
    pach_overlay_fill_rect(surface, x, y, 34, 34, PACH_COLOR_ICON_BG);
    pach_overlay_fill_rect(surface, x, y, 34, 1, accent);
    pach_overlay_fill_rect(surface, x, y + 33, 34, 1, accent);
    pach_overlay_fill_rect(surface, x, y, 1, 34, accent);
    pach_overlay_fill_rect(surface, x + 33, y, 1, 34, accent);

    pach_overlay_fill_rect(surface, x + 10, y + 8, 14, 3, accent);
    pach_overlay_fill_rect(surface, x + 11, y + 11, 12, 7, accent);
    pach_overlay_fill_rect(surface, x + 7, y + 10, 3, 6, accent);
    pach_overlay_fill_rect(surface, x + 24, y + 10, 3, 6, accent);
    pach_overlay_fill_rect(surface, x + 8, y + 15, 4, 2, accent);
    pach_overlay_fill_rect(surface, x + 22, y + 15, 4, 2, accent);
    pach_overlay_fill_rect(surface, x + 15, y + 18, 4, 6, accent);
    pach_overlay_fill_rect(surface, x + 11, y + 24, 12, 3, accent);
}

static void pach_overlay_draw_check(
    const PachOverlaySurface *surface,
    int x,
    int y,
    PachColor accent)
{
    pach_overlay_fill_rect(surface, x, y, 34, 34, PACH_COLOR_ICON_BG);
    pach_overlay_fill_rect(surface, x, y, 34, 1, accent);
    pach_overlay_fill_rect(surface, x, y + 33, 34, 1, accent);
    pach_overlay_fill_rect(surface, x, y, 1, 34, accent);
    pach_overlay_fill_rect(surface, x + 33, y, 1, 34, accent);

    pach_overlay_fill_rect(surface, x + 8, y + 17, 4, 4, accent);
    pach_overlay_fill_rect(surface, x + 11, y + 20, 4, 4, accent);
    pach_overlay_fill_rect(surface, x + 14, y + 23, 4, 4, accent);
    pach_overlay_fill_rect(surface, x + 17, y + 20, 4, 4, accent);
    pach_overlay_fill_rect(surface, x + 20, y + 17, 4, 4, accent);
    pach_overlay_fill_rect(surface, x + 23, y + 14, 4, 4, accent);
    pach_overlay_fill_rect(surface, x + 26, y + 11, 3, 4, accent);
}

static int pach_overlay_render(
    const PachOverlay *overlay,
    const PachNotification *notification)
{
    PachOverlaySurface surface;
    PachColor accent;
    char title_line_one[PACH_TITLE_LINE_CAPACITY];
    char title_line_two[PACH_TITLE_LINE_CAPACITY];
    char value_text[20];
    const char *label;
    const u16 *badge_pixels = NULL;
    int panel_width;
    int panel_x;
    int panel_y;
    int title_characters;
    int value_characters;
    int value_width;
    int result;

    if (overlay == NULL || notification == NULL) {
        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    result = pach_overlay_get_surface(&surface);

    if (result < 0) {
        return result;
    }

    panel_width = PACH_PANEL_WIDTH;

    if (panel_width >
        surface.screen_width - PACH_PANEL_MARGIN * 2) {

        panel_width =
            surface.screen_width - PACH_PANEL_MARGIN * 2;
    }

    panel_x =
        surface.screen_width -
        panel_width -
        PACH_PANEL_MARGIN;

    /*
     * Keep the panel close to the bottom. Drawing starts at VBlank and the
     * LCD scans from top to bottom, giving the CPU most of the frame to
     * finish these direct, opaque writes before scanout reaches the panel.
     */
    panel_y =
        surface.screen_height -
        PACH_PANEL_HEIGHT -
        PACH_PANEL_MARGIN;

    accent = pach_overlay_accent_color(notification);
    label = pach_overlay_label(notification);

    pach_overlay_fill_rect(
        &surface,
        panel_x,
        panel_y,
        panel_width,
        PACH_PANEL_HEIGHT,
        PACH_COLOR_PANEL
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x,
        panel_y,
        4,
        PACH_PANEL_HEIGHT,
        accent
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x + 4,
        panel_y,
        panel_width - 4,
        1,
        PACH_COLOR_PANEL_EDGE
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x + 4,
        panel_y + PACH_PANEL_HEIGHT - 1,
        panel_width - 4,
        1,
        PACH_COLOR_PANEL_EDGE
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x + 4,
        panel_y + PACH_PANEL_HEIGHT - 3,
        panel_width - 4,
        2,
        accent
    );

    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        pach_overlay_draw_check(
            &surface,
            panel_x + 9,
            panel_y + 12,
            accent
        );
    } else {
        badge_pixels = NULL;

        if (overlay->config != NULL &&
            overlay->config->show_badges) {

            badge_pixels = pach_badge_get_pixels(
                overlay->badge_pack,
                notification->achievement_id
            );
        }

        if (badge_pixels != NULL) {
            pach_overlay_draw_badge(
                &surface,
                panel_x + 9,
                panel_y + 12,
                32,
                badge_pixels,
                0,
                accent
            );
        } else {
            pach_overlay_draw_trophy(
                &surface,
                panel_x + 9,
                panel_y + 12,
                accent
            );
        }
    }

    pach_overlay_draw_text(
        &surface,
        panel_x + PACH_CONTENT_LEFT,
        panel_y + 7,
        label,
        24,
        accent
    );

    pach_overlay_build_value_text(
        notification,
        value_text
    );

    value_characters =
        pach_overlay_string_length(value_text);

    value_width =
        value_characters * PACH_FONT_ADVANCE + 6;

    pach_overlay_fill_rect(
        &surface,
        panel_x + panel_width -
            PACH_CONTENT_RIGHT - value_width,
        panel_y + 4,
        value_width,
        13,
        PACH_COLOR_ICON_BG
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + panel_width -
            PACH_CONTENT_RIGHT - value_width + 3,
        panel_y + 7,
        value_text,
        value_characters,
        PACH_COLOR_TEXT
    );

    title_characters =
        (panel_width -
         PACH_CONTENT_LEFT -
         PACH_CONTENT_RIGHT) /
        PACH_FONT_ADVANCE;

    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        pach_overlay_draw_text(
            &surface,
            panel_x + PACH_CONTENT_LEFT,
            panel_y + 27,
            notification->title,
            title_characters,
            PACH_COLOR_TEXT
        );

        pach_overlay_draw_text(
            &surface,
            panel_x + PACH_CONTENT_LEFT,
            panel_y + 40,
            "READY FOR THIS GAME",
            title_characters,
            PACH_COLOR_SUBTEXT
        );
    } else {
        pach_overlay_wrap_title(
            notification->title,
            title_characters,
            title_line_one,
            title_line_two
        );

        pach_overlay_draw_text(
            &surface,
            panel_x + PACH_CONTENT_LEFT,
            panel_y + 27,
            title_line_one,
            title_characters,
            PACH_COLOR_TEXT
        );

        pach_overlay_draw_text(
            &surface,
            panel_x + PACH_CONTENT_LEFT,
            panel_y + 40,
            title_line_two,
            title_characters,
            PACH_COLOR_SUBTEXT
        );
    }

    return PACH_OVERLAY_OK;
}


static int pach_overlay_append_text_at(
    char *output,
    int capacity,
    int *position,
    const char *text)
{
    int index = 0;

    if (output == NULL || position == NULL || text == NULL ||
        capacity <= 0) {

        return -1;
    }

    while (text[index] != '\0' && *position + 1 < capacity) {
        output[*position] = text[index];
        ++(*position);
        ++index;
    }

    output[*position] = '\0';
    return 0;
}

static int pach_overlay_append_u32_at(
    char *output,
    int capacity,
    int *position,
    u32 value)
{
    char digits[10];
    int count = 0;
    int index;

    if (output == NULL || position == NULL || capacity <= 0) {
        return -1;
    }

    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value > 0 && count < 10);

    for (index = count - 1;
         index >= 0 && *position + 1 < capacity;
         --index) {

        output[*position] = digits[index];
        ++(*position);
    }

    output[*position] = '\0';
    return 0;
}

static int pach_overlay_profile_contains_id(
    const volatile PachProfile *profile,
    u32 achievement_id)
{
    u32 count;
    u32 index;

    if (profile == NULL || achievement_id == 0) {
        return 0;
    }

    count = profile->unlocked_count;

    if (count > PACH_ACHIEVEMENT_MAX) {
        count = PACH_ACHIEVEMENT_MAX;
    }

    for (index = 0; index < count; ++index) {
        if (profile->unlocked_ids[index] == achievement_id) {
            return 1;
        }
    }

    return 0;
}

static PachColor pach_overlay_type_color(u8 type)
{
    PachColor color;

    if (type == 1) {
        color.red = 50;
        color.green = 205;
        color.blue = 183;
        return color;
    }

    if (type == 2) {
        color.red = 245;
        color.green = 158;
        color.blue = 54;
        return color;
    }

    if (type == 3) {
        color.red = 247;
        color.green = 205;
        color.blue = 70;
        return color;
    }

    color.red = 91;
    color.green = 157;
    color.blue = 255;
    return color;
}

static const char *pach_overlay_type_label(u8 type)
{
    if (type == 1) {
        return "PROGRESSION";
    }

    if (type == 2) {
        return "MISSABLE";
    }

    if (type == 3) {
        return "WIN CONDITION";
    }

    return "STANDARD";
}

static void pach_overlay_menu_clamp_selection(
    PachOverlay *overlay
);

static int pach_overlay_is_game_thread(
    const SceKernelThreadInfo *info)
{
    if (info == NULL) {
        return 0;
    }

    /*
     * Suspend only normal user-game threads. VSH and USB/WLAN threads use
     * different high attribute bits and must remain alive so HOME and system
     * services continue to work while the achievement menu is open.
     */
    return
        (info->attr & PACH_THREAD_CLASS_MASK) ==
        PSP_THREAD_ATTR_USER;
}

static void pach_overlay_resume_game_threads(
    PachOverlay *overlay)
{
    if (overlay == NULL) {
        return;
    }

    while (overlay->suspended_thread_count > 0) {
        SceUID thread_id;

        --overlay->suspended_thread_count;
        thread_id = overlay->suspended_threads[
            overlay->suspended_thread_count
        ];

        overlay->suspended_threads[
            overlay->suspended_thread_count
        ] = -1;

        if (thread_id >= 0) {
            (void)sceKernelResumeThread(thread_id);
        }
    }

    overlay->game_paused = 0;
}

static int pach_overlay_suspend_game_threads(
    PachOverlay *overlay)
{
    SceUID thread_ids[PACH_OVERLAY_MAX_SUSPENDED_THREADS];
    SceUID current_thread;
    int thread_count = 0;
    int result;
    int index;

    if (overlay == NULL) {
        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    pach_overlay_resume_game_threads(overlay);

    result = sceKernelGetThreadmanIdList(
        SCE_KERNEL_TMID_Thread,
        thread_ids,
        sizeof(thread_ids),
        &thread_count
    );

    if (result < 0) {
        return result;
    }

    if (thread_count < 0) {
        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    if (thread_count >
        (int)PACH_OVERLAY_MAX_SUSPENDED_THREADS) {

        thread_count =
            (int)PACH_OVERLAY_MAX_SUSPENDED_THREADS;
    }

    current_thread = sceKernelGetThreadId();

    for (index = 0; index < thread_count; ++index) {
        SceKernelThreadInfo info;
        SceUID thread_id = thread_ids[index];

        if (thread_id < 0 || thread_id == current_thread) {
            continue;
        }

        info.size = sizeof(info);

        if (sceKernelReferThreadStatus(
                thread_id,
                &info) < 0) {

            continue;
        }

        if (!pach_overlay_is_game_thread(&info)) {
            continue;
        }

        if ((info.status &
             (PSP_THREAD_SUSPEND |
              PSP_THREAD_STOPPED |
              PSP_THREAD_KILLED)) != 0) {

            continue;
        }

        if (sceKernelSuspendThread(thread_id) < 0) {
            continue;
        }

        overlay->suspended_threads[
            overlay->suspended_thread_count
        ] = thread_id;

        ++overlay->suspended_thread_count;

        if (overlay->suspended_thread_count >=
            PACH_OVERLAY_MAX_SUSPENDED_THREADS) {

            break;
        }
    }

    if (overlay->suspended_thread_count == 0) {
        return PACH_OVERLAY_ERROR_DISPLAY;
    }

    overlay->game_paused = 1;

    return (int)overlay->suspended_thread_count;
}

static void pach_overlay_close_menu_internal(
    PachOverlay *overlay)
{
    if (overlay == NULL) {
        return;
    }

    overlay->menu_hold_frames = 0;
    pach_overlay_resume_game_threads(overlay);
    overlay->menu_open = 0;
}

static int pach_overlay_open_menu_internal(
    PachOverlay *overlay)
{
    int result;

    if (overlay == NULL) {
        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    overlay->menu_open = 1;

    result = pach_overlay_suspend_game_threads(overlay);

    if (result < 0) {
        pach_overlay_resume_game_threads(overlay);
        overlay->menu_open = 0;
        return result;
    }

    /*
     * Let any display switch already queued by the game finish before the
     * first menu frame is drawn. With the game threads suspended, later
     * frames remain stable until the menu is closed.
     */
    (void)sceDisplayWaitVblankStart();

    pach_overlay_menu_clamp_selection(overlay);

    return PACH_OVERLAY_OK;
}

static int pach_overlay_menu_available(
    const PachOverlay *overlay)
{
    if (overlay == NULL ||
        overlay->config == NULL ||
        !overlay->config->menu_enabled ||
        overlay->package_info == NULL ||
        overlay->badge_pack == NULL ||
        overlay->achievement_engine == NULL ||
        overlay->profile == NULL ||
        overlay->profile_ready == NULL ||
        !*overlay->profile_ready) {

        return 0;
    }

    return
        overlay->package_info->loaded &&
        overlay->achievement_engine->initialized &&
        overlay->profile->loaded &&
        overlay->package_info->achievement_count > 0;
}

static int pach_overlay_filter_matches(
    const PachOverlay *overlay,
    u32 achievement_index)
{
    const PachAchievementDefinition *definition;
    int unlocked;

    if (overlay == NULL ||
        overlay->package_info == NULL ||
        achievement_index >=
            overlay->package_info->achievement_count) {

        return 0;
    }

    if (overlay->menu_filter == PACH_OVERLAY_FILTER_ALL) {
        return 1;
    }

    definition = &overlay->package_info->achievements[
        achievement_index
    ];

    unlocked = pach_overlay_profile_contains_id(
        overlay->profile,
        definition->id
    );

    if (overlay->menu_filter == PACH_OVERLAY_FILTER_LOCKED) {
        return !unlocked;
    }

    if (overlay->menu_filter == PACH_OVERLAY_FILTER_UNLOCKED) {
        return unlocked;
    }

    return 1;
}

static u32 pach_overlay_filtered_count(
    const PachOverlay *overlay)
{
    u32 count = 0;
    u32 index;

    if (overlay == NULL || overlay->package_info == NULL) {
        return 0;
    }

    for (index = 0;
         index < overlay->package_info->achievement_count;
         ++index) {

        if (pach_overlay_filter_matches(overlay, index)) {
            ++count;
        }
    }

    return count;
}

static int pach_overlay_filtered_index_at(
    const PachOverlay *overlay,
    u32 filtered_position,
    u32 *achievement_index)
{
    u32 position = 0;
    u32 index;

    if (overlay == NULL ||
        overlay->package_info == NULL ||
        achievement_index == NULL) {

        return 0;
    }

    for (index = 0;
         index < overlay->package_info->achievement_count;
         ++index) {

        if (!pach_overlay_filter_matches(overlay, index)) {
            continue;
        }

        if (position == filtered_position) {
            *achievement_index = index;
            return 1;
        }

        ++position;
    }

    return 0;
}

static const char *pach_overlay_filter_label(
    u32 filter)
{
    if (filter == PACH_OVERLAY_FILTER_LOCKED) {
        return "LOCKED";
    }

    if (filter == PACH_OVERLAY_FILTER_UNLOCKED) {
        return "UNLOCKED";
    }

    return "ALL";
}

static void pach_overlay_menu_clamp_selection(
    PachOverlay *overlay)
{
    u32 count;

    if (overlay == NULL || overlay->package_info == NULL) {
        return;
    }

    count = pach_overlay_filtered_count(overlay);

    if (count == 0) {
        overlay->menu_selected_index = 0;
        overlay->menu_first_visible = 0;
        return;
    }

    if (overlay->menu_selected_index >= count) {
        overlay->menu_selected_index = count - 1u;
    }

    if (overlay->menu_selected_index < overlay->menu_first_visible) {
        overlay->menu_first_visible = overlay->menu_selected_index;
    }

    if (overlay->menu_selected_index >=
        overlay->menu_first_visible +
        PACH_OVERLAY_MENU_VISIBLE_ROWS) {

        overlay->menu_first_visible =
            overlay->menu_selected_index -
            PACH_OVERLAY_MENU_VISIBLE_ROWS + 1u;
    }

    if (count <= PACH_OVERLAY_MENU_VISIBLE_ROWS) {
        overlay->menu_first_visible = 0;
    } else if (overlay->menu_first_visible >
               count - PACH_OVERLAY_MENU_VISIBLE_ROWS) {

        overlay->menu_first_visible =
            count - PACH_OVERLAY_MENU_VISIBLE_ROWS;
    }
}

static void pach_overlay_menu_move(
    PachOverlay *overlay,
    int amount)
{
    u32 count;
    int target;

    if (overlay == NULL || overlay->package_info == NULL) {
        return;
    }

    count = pach_overlay_filtered_count(overlay);

    if (count == 0) {
        return;
    }

    target = (int)overlay->menu_selected_index + amount;

    if (target < 0) {
        target = 0;
    }

    if ((u32)target >= count) {
        target = (int)count - 1;
    }

    overlay->menu_selected_index = (u32)target;
    pach_overlay_menu_clamp_selection(overlay);
}

static void pach_overlay_menu_jump(
    PachOverlay *overlay,
    int to_end)
{
    u32 count;

    if (overlay == NULL) {
        return;
    }

    count = pach_overlay_filtered_count(overlay);

    if (count == 0) {
        return;
    }

    overlay->menu_selected_index = to_end
        ? count - 1u
        : 0u;

    pach_overlay_menu_clamp_selection(overlay);
}

static void pach_overlay_menu_cycle_filter(
    PachOverlay *overlay)
{
    u32 attempt;

    if (overlay == NULL) {
        return;
    }

    for (attempt = 0; attempt < 3u; ++attempt) {
        overlay->menu_filter =
            (overlay->menu_filter + 1u) % 3u;

        if (pach_overlay_filtered_count(overlay) > 0) {
            break;
        }
    }

    overlay->menu_selected_index = 0;
    overlay->menu_first_visible = 0;
    pach_overlay_menu_clamp_selection(overlay);
}

static void pach_overlay_update_menu_input(
    PachOverlay *overlay)
{
    SceCtrlData pad;
    u32 buttons;
    u32 pressed;
    int combo_held;

    if (overlay == NULL) {
        return;
    }

    if (sceCtrlPeekBufferPositive(&pad, 1) <= 0) {
        return;
    }

    buttons = pad.Buttons;
    pressed = buttons & ~overlay->previous_buttons;
    combo_held =
        overlay->config != NULL &&
        overlay->config->menu_buttons != 0 &&
        (buttons & overlay->config->menu_buttons) ==
            overlay->config->menu_buttons;

    if (!pach_overlay_menu_available(overlay)) {
        pach_overlay_close_menu_internal(overlay);
        overlay->previous_buttons = buttons;
        return;
    }

    if (combo_held) {
        u32 hold_frames = pach_config_ms_to_frames(
            overlay->config->menu_hold_ms
        );

        if (overlay->menu_hold_frames < hold_frames) {
            ++overlay->menu_hold_frames;
        } else if (overlay->menu_hold_frames == hold_frames) {
            if (overlay->menu_open) {
                pach_overlay_close_menu_internal(overlay);
            } else {
                (void)pach_overlay_open_menu_internal(overlay);
            }

            overlay->menu_hold_frames = hold_frames + 1u;
            overlay->previous_buttons = buttons;
            return;
        }
    } else {
        overlay->menu_hold_frames = 0;
    }

    if (!overlay->menu_open) {
        overlay->previous_buttons = buttons;
        return;
    }

    if ((buttons & PSP_CTRL_HOME) != 0 ||
        (pressed & PSP_CTRL_CIRCLE) != 0) {

        pach_overlay_close_menu_internal(overlay);
    } else if ((pressed & PSP_CTRL_TRIANGLE) != 0) {
        pach_overlay_menu_cycle_filter(overlay);
    } else if ((pressed & PSP_CTRL_LTRIGGER) != 0) {
        pach_overlay_menu_jump(overlay, 0);
    } else if ((pressed & PSP_CTRL_RTRIGGER) != 0) {
        pach_overlay_menu_jump(overlay, 1);
    } else if ((pressed & PSP_CTRL_UP) != 0) {
        pach_overlay_menu_move(overlay, -1);
    } else if ((pressed & PSP_CTRL_DOWN) != 0) {
        pach_overlay_menu_move(overlay, 1);
    } else if ((pressed & PSP_CTRL_LEFT) != 0) {
        pach_overlay_menu_move(
            overlay,
            -(int)PACH_OVERLAY_MENU_VISIBLE_ROWS
        );
    } else if ((pressed & PSP_CTRL_RIGHT) != 0) {
        pach_overlay_menu_move(
            overlay,
            (int)PACH_OVERLAY_MENU_VISIBLE_ROWS
        );
    }

    overlay->previous_buttons = buttons;
}

static void pach_overlay_draw_state_box(
    const PachOverlaySurface *surface,
    int x,
    int y,
    int unlocked,
    PachColor accent)
{
    PachColor border = unlocked
        ? PACH_COLOR_MENU_GREEN
        : PACH_COLOR_MENU_LOCKED;

    pach_overlay_fill_rect(surface, x, y, 11, 11, border);
    pach_overlay_fill_rect(surface, x + 1, y + 1, 9, 9,
        PACH_COLOR_MENU_BG);

    if (!unlocked) {
        return;
    }

    pach_overlay_fill_rect(surface, x + 2, y + 5, 2, 2, accent);
    pach_overlay_fill_rect(surface, x + 4, y + 7, 2, 2, accent);
    pach_overlay_fill_rect(surface, x + 6, y + 5, 2, 2, accent);
    pach_overlay_fill_rect(surface, x + 8, y + 3, 2, 2, accent);
}

static void pach_overlay_build_count_text(
    char output[32],
    u32 current,
    u32 total)
{
    int position = 0;

    output[0] = '\0';

    (void)pach_overlay_append_u32_at(
        output, 32, &position, current
    );
    (void)pach_overlay_append_text_at(
        output, 32, &position, " / "
    );
    (void)pach_overlay_append_u32_at(
        output, 32, &position, total
    );
}

static void pach_overlay_build_progress_text(
    char output[20],
    const PachAchievementProgress *progress)
{
    int position = 0;

    output[0] = '\0';

    if (progress == NULL ||
        !progress->available ||
        progress->target == 0) {

        return;
    }

    (void)pach_overlay_append_u32_at(
        output, 20, &position, progress->current
    );
    (void)pach_overlay_append_text_at(
        output, 20, &position, "/"
    );
    (void)pach_overlay_append_u32_at(
        output, 20, &position, progress->target
    );
}

static void pach_overlay_build_filter_text(
    char output[32],
    u32 filter,
    u32 filtered_count)
{
    int position = 0;

    output[0] = '\0';

    (void)pach_overlay_append_text_at(
        output,
        32,
        &position,
        pach_overlay_filter_label(filter)
    );
    (void)pach_overlay_append_text_at(
        output, 32, &position, " "
    );
    (void)pach_overlay_append_u32_at(
        output, 32, &position, filtered_count
    );
}

static void pach_overlay_build_meta_text(
    char output[80],
    const PachAchievementDefinition *definition,
    int unlocked,
    const PachAchievementProgress *progress)
{
    int position = 0;

    output[0] = '\0';

    (void)pach_overlay_append_text_at(
        output,
        80,
        &position,
        pach_overlay_type_label(definition->type)
    );
    (void)pach_overlay_append_text_at(
        output, 80, &position, "  "
    );
    (void)pach_overlay_append_u32_at(
        output, 80, &position, definition->points
    );
    (void)pach_overlay_append_text_at(
        output, 80, &position, " PTS  "
    );
    (void)pach_overlay_append_text_at(
        output,
        80,
        &position,
        unlocked ? "UNLOCKED" : "LOCKED"
    );

    if (!unlocked &&
        progress != NULL &&
        progress->available &&
        progress->target > 0) {

        (void)pach_overlay_append_text_at(
            output, 80, &position, "  PROGRESS "
        );
        (void)pach_overlay_append_u32_at(
            output, 80, &position, progress->current
        );
        (void)pach_overlay_append_text_at(
            output, 80, &position, "/"
        );
        (void)pach_overlay_append_u32_at(
            output, 80, &position, progress->target
        );
    }
}

static int pach_overlay_render_menu(
    PachOverlay *overlay)
{
    PachOverlaySurface surface;
    const PachPackageInfo *package_info;
    const PachAchievementDefinition *selected_definition;
    const char *selected_description;
    char achievement_progress_text[32];
    char points_progress_text[32];
    char filter_text[32];
    char meta_text[80];
    char value_text[20];
    char description_line_one[PACH_TITLE_LINE_CAPACITY];
    char description_line_two[PACH_TITLE_LINE_CAPACITY];
    u32 achievement_count;
    u32 filtered_count;
    u32 selected_achievement_index = 0;
    u32 unlocked_count = 0;
    u32 total_points = 0;
    u32 unlocked_points = 0;
    u32 index;
    u32 row;
    int panel_x;
    int panel_y;
    int panel_width;
    int panel_height;
    int list_y;
    int max_title_characters;
    int description_characters;
    int selected_unlocked;
    PachAchievementProgress selected_progress;
    int result;

    if (overlay == NULL || !pach_overlay_menu_available(overlay)) {
        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    result = pach_overlay_get_surface(&surface);

    if (result < 0) {
        return result;
    }

    package_info = overlay->package_info;
    achievement_count = package_info->achievement_count;
    pach_overlay_menu_clamp_selection(overlay);
    filtered_count = pach_overlay_filtered_count(overlay);

    if (filtered_count == 0 ||
        !pach_overlay_filtered_index_at(
            overlay,
            overlay->menu_selected_index,
            &selected_achievement_index)) {

        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    for (index = 0; index < achievement_count; ++index) {
        const PachAchievementDefinition *definition =
            &package_info->achievements[index];

        total_points += definition->points;

        if (pach_overlay_profile_contains_id(
                overlay->profile,
                definition->id)) {

            ++unlocked_count;
            unlocked_points += definition->points;
        }
    }

    panel_x = PACH_MENU_MARGIN_X;
    panel_y = PACH_MENU_MARGIN_Y;
    panel_width = surface.screen_width - PACH_MENU_MARGIN_X * 2;
    panel_height = surface.screen_height - PACH_MENU_MARGIN_Y * 2;
    list_y = panel_y + PACH_MENU_LIST_OFFSET_Y;

    pach_overlay_fill_rect(
        &surface,
        panel_x,
        panel_y,
        panel_width,
        panel_height,
        PACH_COLOR_MENU_BG
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x,
        panel_y,
        panel_width,
        28,
        PACH_COLOR_MENU_HEADER
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x,
        panel_y,
        4,
        panel_height,
        PACH_COLOR_MENU_GREEN
    );

    pach_overlay_fill_rect(
        &surface,
        panel_x,
        panel_y + panel_height - 1,
        panel_width,
        1,
        PACH_COLOR_PANEL_EDGE
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + 12,
        panel_y + 9,
        "PSPACHIEVEMENTSNG",
        24,
        PACH_COLOR_TEXT
    );

    pach_overlay_build_filter_text(
        filter_text,
        overlay->menu_filter,
        filtered_count
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + 192,
        panel_y + 9,
        filter_text,
        24,
        PACH_COLOR_SUBTEXT
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + panel_width - 82,
        panel_y + 9,
        package_info->game_id,
        11,
        PACH_COLOR_MENU_GREEN
    );

    pach_overlay_build_count_text(
        achievement_progress_text,
        unlocked_count,
        achievement_count
    );

    pach_overlay_build_count_text(
        points_progress_text,
        unlocked_points,
        total_points
    );

    {
        const int summary_gap = 8;
        const int summary_width =
            (panel_width - 24 - summary_gap) / 2;
        const int left_x = panel_x + 12;
        const int right_x = left_x + summary_width + summary_gap;
        const int summary_y = panel_y + 33;

        pach_overlay_fill_rect(
            &surface,
            left_x,
            summary_y,
            summary_width,
            24,
            PACH_COLOR_MENU_HEADER
        );

        pach_overlay_fill_rect(
            &surface,
            right_x,
            summary_y,
            summary_width,
            24,
            PACH_COLOR_MENU_HEADER
        );

        pach_overlay_fill_rect(
            &surface,
            left_x,
            summary_y,
            3,
            24,
            PACH_COLOR_MENU_GREEN
        );

        pach_overlay_fill_rect(
            &surface,
            right_x,
            summary_y,
            3,
            24,
            PACH_COLOR_MENU_GREEN
        );

        pach_overlay_draw_text(
            &surface,
            left_x + 9,
            summary_y + 3,
            "ACHIEVEMENTS",
            20,
            PACH_COLOR_SUBTEXT
        );

        pach_overlay_draw_text(
            &surface,
            left_x + 9,
            summary_y + 14,
            achievement_progress_text,
            20,
            PACH_COLOR_TEXT
        );

        pach_overlay_draw_text(
            &surface,
            right_x + 9,
            summary_y + 3,
            "POINTS",
            20,
            PACH_COLOR_SUBTEXT
        );

        pach_overlay_draw_text(
            &surface,
            right_x + 9,
            summary_y + 14,
            points_progress_text,
            20,
            PACH_COLOR_TEXT
        );
    }

    pach_overlay_fill_rect(
        &surface,
        panel_x + 12,
        panel_y + 59,
        panel_width - 24,
        3,
        PACH_COLOR_MENU_TRACK
    );

    if (achievement_count > 0) {
        int fill_width = (int)(
            (u32)(panel_width - 24) * unlocked_count /
            achievement_count
        );

        if (fill_width > 0) {
            pach_overlay_fill_rect(
                &surface,
                panel_x + 12,
                panel_y + 59,
                fill_width,
                3,
                PACH_COLOR_MENU_GREEN
            );
        }
    }

    max_title_characters = (panel_width - 112) / PACH_FONT_ADVANCE;

    for (row = 0;
         row < PACH_OVERLAY_MENU_VISIBLE_ROWS;
         ++row) {

        u32 filtered_position =
            overlay->menu_first_visible + row;

        u32 achievement_index;
        const PachAchievementDefinition *definition;
        const char *title;
        PachColor accent;
        PachColor title_color;
        int row_y;
        int unlocked;
        int points_length;
        PachAchievementProgress row_progress;
        const u16 *badge_pixels;

        if (filtered_position >= filtered_count ||
            !pach_overlay_filtered_index_at(
                overlay,
                filtered_position,
                &achievement_index)) {

            break;
        }

        definition = &package_info->achievements[achievement_index];
        title = pach_package_get_string(
            package_info,
            definition->title_offset
        );

        if (title == NULL || title[0] == '\0') {
            title = "Achievement";
        }

        row_y = list_y + (int)row * PACH_MENU_ROW_HEIGHT;
        unlocked = pach_overlay_profile_contains_id(
            overlay->profile,
            definition->id
        );
        accent = pach_overlay_type_color(definition->type);
        title_color = unlocked
            ? PACH_COLOR_TEXT
            : PACH_COLOR_MENU_LOCKED;
        badge_pixels = NULL;

        if (overlay->config != NULL &&
            overlay->config->show_badges) {

            badge_pixels = pach_badge_get_pixels(
                overlay->badge_pack,
                definition->id
            );
        }

        if (filtered_position == overlay->menu_selected_index) {
            pach_overlay_fill_rect(
                &surface,
                panel_x + 8,
                row_y,
                panel_width - 16,
                PACH_MENU_ROW_HEIGHT - 2,
                PACH_COLOR_MENU_SELECTED
            );
        }

        pach_overlay_fill_rect(
            &surface,
            panel_x + 8,
            row_y,
            3,
            PACH_MENU_ROW_HEIGHT - 2,
            accent
        );

        if (badge_pixels != NULL) {
            pach_overlay_draw_badge(
                &surface,
                panel_x + 15,
                row_y,
                16,
                badge_pixels,
                !unlocked,
                accent
            );
        } else {
            pach_overlay_draw_state_box(
                &surface,
                panel_x + 18,
                row_y + 4,
                unlocked,
                accent
            );
        }

        pach_overlay_draw_text(
            &surface,
            panel_x + 38,
            row_y + 6,
            title,
            max_title_characters,
            title_color
        );

        row_progress.available = 0;
        row_progress.current = 0;
        row_progress.target = 0;
        row_progress.measured = 0;
        row_progress.reserved = 0;

        if (!unlocked) {
            (void)pach_achievement_engine_get_progress(
                (const PachAchievementEngine *)
                    overlay->achievement_engine,
                achievement_index,
                &row_progress
            );
        }

        if (!unlocked && row_progress.available) {
            pach_overlay_build_progress_text(
                value_text,
                &row_progress
            );
        } else {
            pach_overlay_append_number(
                value_text,
                20,
                definition->points,
                ""
            );
        }

        points_length = pach_overlay_string_length(value_text);

        pach_overlay_draw_text(
            &surface,
            panel_x + panel_width - 18 -
                points_length * PACH_FONT_ADVANCE,
            row_y + 6,
            value_text,
            points_length,
            accent
        );
    }

    if (filtered_count > PACH_OVERLAY_MENU_VISIBLE_ROWS) {
        int track_y = list_y;
        int track_height =
            PACH_MENU_ROW_HEIGHT *
            (int)PACH_OVERLAY_MENU_VISIBLE_ROWS - 2;
        int thumb_height =
            track_height *
            (int)PACH_OVERLAY_MENU_VISIBLE_ROWS /
            (int)filtered_count;
        int thumb_y;

        if (thumb_height < 8) {
            thumb_height = 8;
        }

        thumb_y = track_y +
            (track_height - thumb_height) *
            (int)overlay->menu_first_visible /
            (int)(filtered_count -
                PACH_OVERLAY_MENU_VISIBLE_ROWS);

        pach_overlay_fill_rect(
            &surface,
            panel_x + panel_width - 7,
            track_y,
            2,
            track_height,
            PACH_COLOR_MENU_TRACK
        );

        pach_overlay_fill_rect(
            &surface,
            panel_x + panel_width - 7,
            thumb_y,
            2,
            thumb_height,
            PACH_COLOR_MENU_GREEN
        );
    }

    selected_definition = &package_info->achievements[
        selected_achievement_index
    ];

    selected_unlocked = pach_overlay_profile_contains_id(
        overlay->profile,
        selected_definition->id
    );

    selected_description = pach_package_get_string(
        package_info,
        selected_definition->description_offset
    );

    if (selected_description == NULL ||
        selected_description[0] == '\0') {

        selected_description = "No description.";
    }

    pach_overlay_fill_rect(
        &surface,
        panel_x + 8,
        panel_y + PACH_MENU_DETAIL_Y,
        panel_width - 16,
        1,
        PACH_COLOR_PANEL_EDGE
    );

    selected_progress.available = 0;
    selected_progress.current = 0;
    selected_progress.target = 0;
    selected_progress.measured = 0;
    selected_progress.reserved = 0;

    if (!selected_unlocked) {
        (void)pach_achievement_engine_get_progress(
            (const PachAchievementEngine *)
                overlay->achievement_engine,
            selected_achievement_index,
            &selected_progress
        );
    }

    pach_overlay_build_meta_text(
        meta_text,
        selected_definition,
        selected_unlocked,
        &selected_progress
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + 12,
        panel_y + PACH_MENU_DETAIL_Y + 7,
        meta_text,
        70,
        pach_overlay_type_color(selected_definition->type)
    );

    description_characters = (panel_width - 24) / PACH_FONT_ADVANCE;

    pach_overlay_wrap_title(
        selected_description,
        description_characters,
        description_line_one,
        description_line_two
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + 12,
        panel_y + PACH_MENU_DETAIL_Y + 20,
        description_line_one,
        description_characters,
        PACH_COLOR_TEXT
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + 12,
        panel_y + PACH_MENU_DETAIL_Y + 31,
        description_line_two,
        description_characters,
        PACH_COLOR_SUBTEXT
    );

    pach_overlay_draw_text(
        &surface,
        panel_x + 12,
        panel_y + PACH_MENU_FOOTER_Y,
        "TRI FILTER  UP/DN MOVE  LT/RT PAGE  L/R ENDS  CIRCLE CLOSE",
        70,
        PACH_COLOR_MENU_LOCKED
    );

    return PACH_OVERLAY_OK;
}

void pach_overlay_reset(
    PachOverlay *overlay)
{
    u32 index;

    if (overlay == NULL) {
        return;
    }

    pach_overlay_prepare_stop(overlay);

    overlay->queue = NULL;
    overlay->running = NULL;
    overlay->config = NULL;
    overlay->package_info = NULL;
    overlay->badge_pack = NULL;
    overlay->achievement_engine = NULL;
    overlay->profile = NULL;
    overlay->profile_ready = NULL;

    pach_overlay_clear_notification(
        &overlay->active_notification
    );

    if (overlay->sound_ready) {
        pach_sound_shutdown(&overlay->sound);
    } else {
        pach_sound_reset(&overlay->sound);
    }

    overlay->frames_remaining = 0;
    overlay->previous_buttons = 0;
    overlay->menu_hold_frames = 0;
    overlay->menu_selected_index = 0;
    overlay->menu_first_visible = 0;
    overlay->menu_filter = PACH_OVERLAY_FILTER_ALL;
    overlay->suspended_thread_count = 0;

    for (index = 0;
         index < PACH_OVERLAY_MAX_SUSPENDED_THREADS;
         ++index) {

        overlay->suspended_threads[index] = -1;
    }

    overlay->sound_ready = 0;
    overlay->sound_pending = 0;
    overlay->active = 0;
    overlay->game_paused = 0;
    overlay->menu_open = 0;
    overlay->initialized = 0;
}

int pach_overlay_init(
    PachOverlay *overlay,
    PachNotificationQueue *queue,
    volatile int *running,
    const PachConfig *config,
    const PachPackageInfo *package_info,
    const PachBadgePack *badge_pack,
    const volatile PachAchievementEngine *achievement_engine,
    volatile PachProfile *profile,
    volatile int *profile_ready)
{
    if (overlay == NULL ||
        queue == NULL ||
        running == NULL ||
        config == NULL ||
        package_info == NULL ||
        badge_pack == NULL ||
        achievement_engine == NULL ||
        profile == NULL ||
        profile_ready == NULL ||
        !queue->initialized) {

        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    pach_overlay_reset(overlay);

    overlay->queue = queue;
    overlay->running = running;
    overlay->config = config;
    overlay->package_info = package_info;
    overlay->badge_pack = badge_pack;
    overlay->achievement_engine = achievement_engine;
    overlay->profile = profile;
    overlay->profile_ready = profile_ready;

    if (config->audio_enabled &&
        config->audio_volume_percent > 0 &&
        pach_sound_init(
            &overlay->sound,
            config->audio_volume_percent
        ) >= 0) {

        overlay->sound_ready = 1;
    }

    overlay->initialized = 1;

    return PACH_OVERLAY_OK;
}

void pach_overlay_prepare_stop(
    PachOverlay *overlay)
{
    if (overlay == NULL) {
        return;
    }

    pach_overlay_close_menu_internal(overlay);
}

static int pach_overlay_notification_visible(
    const PachOverlay *overlay,
    const PachNotification *notification)
{
    if (overlay == NULL ||
        overlay->config == NULL ||
        notification == NULL ||
        !overlay->config->notifications_enabled) {

        return 0;
    }

    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        return overlay->config->startup_notification_enabled;
    }

    return 1;
}

static int pach_overlay_notification_sound_enabled(
    const PachOverlay *overlay,
    const PachNotification *notification)
{
    if (overlay == NULL ||
        overlay->config == NULL ||
        notification == NULL ||
        !overlay->config->audio_enabled ||
        overlay->config->audio_volume_percent == 0) {

        return 0;
    }

    if (notification->kind ==
        PACH_NOTIFICATION_KIND_STATUS) {

        return overlay->config->startup_sound_enabled;
    }

    return overlay->config->unlock_sound_enabled;
}

static u32 pach_overlay_notification_frames(
    const PachOverlay *overlay,
    const PachNotification *notification)
{
    u32 milliseconds;

    if (overlay == NULL ||
        overlay->config == NULL ||
        notification == NULL ||
        !pach_overlay_notification_visible(
            overlay,
            notification)) {

        return 1u;
    }

    milliseconds =
        notification->kind == PACH_NOTIFICATION_KIND_STATUS
        ? overlay->config->startup_notification_duration_ms
        : overlay->config->notification_duration_ms;

    return pach_config_ms_to_frames(milliseconds);
}

int pach_overlay_run(
    PachOverlay *overlay)
{
    u32 rendered_selected_index = 0xFFFFFFFFu;
    u32 rendered_first_visible = 0xFFFFFFFFu;
    u32 rendered_filter = 0xFFFFFFFFu;
    int menu_was_open = 0;
    int result;

    if (overlay == NULL ||
        !overlay->initialized ||
        overlay->queue == NULL ||
        overlay->running == NULL ||
        overlay->config == NULL ||
        overlay->package_info == NULL ||
        overlay->profile == NULL ||
        overlay->profile_ready == NULL) {

        return PACH_OVERLAY_ERROR_ARGUMENT;
    }

    while (*overlay->running) {
        result = sceDisplayWaitVblankStart();

        if (!*overlay->running) {
            break;
        }

        if (overlay->sound_ready) {
            pach_sound_tick(&overlay->sound);
        }

        if (result < 0) {
            sceKernelDelayThread(1000);
            continue;
        }

        if (sceDisplayIsForeground() <= 0) {
            if (overlay->menu_open || overlay->game_paused) {
                pach_overlay_close_menu_internal(overlay);
            }

            continue;
        }

        pach_overlay_update_menu_input(overlay);

        if (overlay->menu_open) {
            /*
             * The game framebuffer is frozen while the menu is open.
             * Redrawing the entire screen every VBlank is unnecessary and
             * causes the first scanlines to be modified while the LCD is
             * already scanning them. That made the summary cards at the top
             * appear to float, while the lower list looked stable.
             *
             * Draw only when the menu first opens or the selection/page
             * changes. The completed framebuffer then remains untouched.
             */
            if (!menu_was_open ||
                rendered_selected_index !=
                    overlay->menu_selected_index ||
                rendered_first_visible !=
                    overlay->menu_first_visible ||
                rendered_filter !=
                    overlay->menu_filter) {

                (void)pach_overlay_render_menu(overlay);

                rendered_selected_index =
                    overlay->menu_selected_index;

                rendered_first_visible =
                    overlay->menu_first_visible;

                rendered_filter =
                    overlay->menu_filter;
            }

            menu_was_open = 1;
            continue;
        }

        if (menu_was_open) {
            menu_was_open = 0;
            rendered_selected_index = 0xFFFFFFFFu;
            rendered_first_visible = 0xFFFFFFFFu;
            rendered_filter = 0xFFFFFFFFu;
        }

        if (!overlay->active) {
            result = pach_notification_dequeue(
                overlay->queue,
                &overlay->active_notification
            );

            if (result == PACH_NOTIFICATION_OK) {
                overlay->active = 1;
                overlay->sound_pending =
                    pach_overlay_notification_sound_enabled(
                        overlay,
                        &overlay->active_notification
                    );

                overlay->frames_remaining =
                    pach_overlay_notification_frames(
                        overlay,
                        &overlay->active_notification
                    );
            }
        }

        if (!overlay->active) {
            continue;
        }

        if (pach_overlay_notification_visible(
                overlay,
                &overlay->active_notification)) {

            pach_overlay_render(
                overlay,
                &overlay->active_notification
            );
        }

        if (overlay->sound_pending) {
            if (overlay->sound_ready) {
                PachSoundKind sound_kind =
                    overlay->active_notification.kind ==
                        PACH_NOTIFICATION_KIND_STATUS
                    ? PACH_SOUND_KIND_STATUS
                    : PACH_SOUND_KIND_ACHIEVEMENT;

                (void)pach_sound_play(
                    &overlay->sound,
                    sound_kind
                );
            }

            overlay->sound_pending = 0;
        }

        if (overlay->frames_remaining > 0) {
            --overlay->frames_remaining;
        }

        if (overlay->frames_remaining == 0) {
            pach_overlay_clear_notification(
                &overlay->active_notification
            );

            overlay->sound_pending = 0;
            overlay->active = 0;
        }
    }

    pach_overlay_prepare_stop(overlay);

    if (overlay->sound_ready) {
        pach_sound_shutdown(&overlay->sound);
    }

    pach_overlay_clear_notification(
        &overlay->active_notification
    );

    overlay->frames_remaining = 0;
    overlay->previous_buttons = 0;
    overlay->menu_hold_frames = 0;
    overlay->suspended_thread_count = 0;
    overlay->sound_ready = 0;
    overlay->sound_pending = 0;
    overlay->active = 0;
    overlay->game_paused = 0;
    overlay->menu_open = 0;

    return PACH_OVERLAY_OK;
}
