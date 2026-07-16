#include "pach_badge.h"

#include <stddef.h>

#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>

#define PACH_BADGE_DIRECTORY \
    "ms0:/SEPLUGINS/PSPAchievementsNG/games/"

#define PACH_BADGE_EXTENSION ".pbad"
#define PACH_BADGE_FNV_OFFSET 2166136261u
#define PACH_BADGE_FNV_PRIME 16777619u

static u16 pach_badge_read_u16_le(
    const u8 *data)
{
    return
        (u16)data[0] |
        ((u16)data[1] << 8);
}

static u32 pach_badge_read_u32_le(
    const u8 *data)
{
    return
        (u32)data[0] |
        ((u32)data[1] << 8) |
        ((u32)data[2] << 16) |
        ((u32)data[3] << 24);
}

static u32 pach_badge_checksum_update(
    u32 checksum,
    const u8 *data,
    u32 size)
{
    u32 index;

    for (index = 0; index < size; ++index) {
        checksum ^= (u32)data[index];
        checksum *= PACH_BADGE_FNV_PRIME;
    }

    return checksum;
}

static int pach_badge_read_exact(
    SceUID file,
    u8 *data,
    u32 size)
{
    u32 total = 0;

    if (file < 0 || data == NULL) {
        return PACH_BADGE_ERROR_ARGUMENT;
    }

    while (total < size) {
        int count = sceIoRead(
            file,
            data + total,
            size - total
        );

        if (count <= 0) {
            return PACH_BADGE_ERROR_READ;
        }

        total += (u32)count;
    }

    return PACH_BADGE_OK;
}

static int pach_badge_validate_game_id(
    const char *game_id)
{
    int index;

    if (game_id == NULL) {
        return 0;
    }

    for (index = 0; index < 4; ++index) {
        if (game_id[index] < 'A' ||
            game_id[index] > 'Z') {

            return 0;
        }
    }

    if (game_id[4] != '-') {
        return 0;
    }

    for (index = 5;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        if (game_id[index] < '0' ||
            game_id[index] > '9') {

            return 0;
        }
    }

    return
        game_id[PACH_GAME_ID_LENGTH] == '\0';
}

static int pach_badge_game_ids_equal(
    const char *left,
    const char *right)
{
    int index;

    if (left == NULL || right == NULL) {
        return 0;
    }

    for (index = 0;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        if (left[index] != right[index]) {
            return 0;
        }
    }

    return
        left[PACH_GAME_ID_LENGTH] == '\0' &&
        right[PACH_GAME_ID_LENGTH] == '\0';
}

static int pach_badge_append_text(
    char *output,
    u32 output_capacity,
    u32 *position,
    const char *text)
{
    u32 index = 0;

    if (output == NULL ||
        position == NULL ||
        text == NULL ||
        output_capacity == 0) {

        return PACH_BADGE_ERROR_ARGUMENT;
    }

    while (text[index] != '\0') {
        if (*position + 1u >= output_capacity) {
            return PACH_BADGE_ERROR_PATH;
        }

        output[*position] = text[index];
        ++(*position);
        ++index;
    }

    output[*position] = '\0';
    return PACH_BADGE_OK;
}

static int pach_badge_has_duplicate_id(
    const PachBadgePack *pack,
    u32 current_index,
    u32 achievement_id)
{
    u32 index;

    for (index = 0; index < current_index; ++index) {
        if (pack->records[index].achievement_id ==
            achievement_id) {

            return 1;
        }
    }

    return 0;
}

void pach_badge_reset(
    PachBadgePack *pack)
{
    u32 index;

    if (pack == NULL) {
        return;
    }

    pack->format_version = 0;
    pack->header_size = 0;

    for (index = 0;
         index < PACH_GAME_ID_CAPACITY;
         ++index) {

        pack->game_id[index] = '\0';
    }

    pack->width = 0;
    pack->height = 0;
    pack->pixel_format = 0;
    pack->record_size = 0;
    pack->badge_count = 0;
    pack->pixel_data_size = 0;
    pack->pack_id = 0;

    for (index = 0;
         index < PACH_ACHIEVEMENT_MAX;
         ++index) {

        pack->records[index].achievement_id = 0;
        pack->records[index].pixel_offset = 0;
        pack->records[index].pixel_size = 0;
    }

    pack->loaded = 0;
}

int pach_badge_build_path(
    const char *game_id,
    char *output,
    u32 output_capacity)
{
    u32 position = 0;
    int result;

    if (game_id == NULL || output == NULL) {
        return PACH_BADGE_ERROR_ARGUMENT;
    }

    if (!pach_badge_validate_game_id(game_id)) {
        return PACH_BADGE_ERROR_GAME_ID;
    }

    output[0] = '\0';

    result = pach_badge_append_text(
        output,
        output_capacity,
        &position,
        PACH_BADGE_DIRECTORY
    );

    if (result < 0) {
        return result;
    }

    result = pach_badge_append_text(
        output,
        output_capacity,
        &position,
        game_id
    );

    if (result < 0) {
        return result;
    }

    return pach_badge_append_text(
        output,
        output_capacity,
        &position,
        PACH_BADGE_EXTENSION
    );
}

int pach_badge_load(
    const char *path,
    const char *expected_game_id,
    PachBadgePack *pack)
{
    u8 header[PACH_BADGE_HEADER_SIZE];
    u8 record_data[
        PACH_ACHIEVEMENT_MAX *
        PACH_BADGE_RECORD_SIZE
    ];

    SceIoStat stat;
    SceUID file;

    u32 stored_checksum;
    u32 calculated_checksum;
    u32 expected_pixel_size;
    u32 body_size;
    u32 index;

    int result;

    if (path == NULL ||
        expected_game_id == NULL ||
        pack == NULL) {

        return PACH_BADGE_ERROR_ARGUMENT;
    }

    pach_badge_reset(pack);

    if (!pach_badge_validate_game_id(
            expected_game_id)) {

        return PACH_BADGE_ERROR_GAME_ID;
    }

    if (sceIoGetstat(path, &stat) < 0) {
        return PACH_BADGE_NOT_FOUND;
    }

    file = sceIoOpen(
        path,
        PSP_O_RDONLY,
        0777
    );

    if (file < 0) {
        return PACH_BADGE_ERROR_OPEN;
    }

    result = pach_badge_read_exact(
        file,
        header,
        sizeof(header)
    );

    if (result < 0) {
        sceIoClose(file);
        return result;
    }

    if (header[0] != 'P' ||
        header[1] != 'B' ||
        header[2] != 'A' ||
        header[3] != 'D') {

        sceIoClose(file);
        return PACH_BADGE_ERROR_MAGIC;
    }

    pack->format_version =
        pach_badge_read_u16_le(&header[4]);

    pack->header_size =
        pach_badge_read_u16_le(&header[6]);

    if (pack->format_version !=
        PACH_BADGE_FORMAT_VERSION) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_VERSION;
    }

    if (pack->header_size !=
        PACH_BADGE_HEADER_SIZE) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_HEADER;
    }

    for (index = 0;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        pack->game_id[index] =
            (char)header[8 + index];
    }

    pack->game_id[PACH_GAME_ID_LENGTH] = '\0';

    if (header[18] != 0 ||
        header[19] != 0 ||
        !pach_badge_validate_game_id(
            pack->game_id)) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_GAME_ID;
    }

    if (!pach_badge_game_ids_equal(
            pack->game_id,
            expected_game_id)) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_GAME_MISMATCH;
    }

    pack->width =
        pach_badge_read_u16_le(&header[20]);

    pack->height =
        pach_badge_read_u16_le(&header[22]);

    pack->pixel_format =
        pach_badge_read_u16_le(&header[24]);

    pack->record_size =
        pach_badge_read_u16_le(&header[26]);

    pack->badge_count =
        pach_badge_read_u32_le(&header[28]);

    pack->pixel_data_size =
        pach_badge_read_u32_le(&header[32]);

    pack->pack_id =
        pach_badge_read_u32_le(&header[36]);

    stored_checksum =
        pach_badge_read_u32_le(&header[40]);

    if (pack->width != PACH_BADGE_WIDTH ||
        pack->height != PACH_BADGE_HEIGHT ||
        pack->pixel_format !=
            PACH_BADGE_PIXEL_FORMAT_RGB565 ||
        pack->record_size !=
            PACH_BADGE_RECORD_SIZE) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_FORMAT;
    }

    if (pack->badge_count == 0 ||
        pack->badge_count >
            PACH_ACHIEVEMENT_MAX) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_COUNT;
    }

    expected_pixel_size =
        pack->badge_count *
        PACH_BADGE_PIXELS_PER_IMAGE *
        2u;

    if (pack->pixel_data_size !=
        expected_pixel_size ||
        pack->pixel_data_size >
            PACH_BADGE_PIXEL_CAPACITY * 2u) {

        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_FORMAT;
    }

    if (pack->pack_id == 0) {
        sceIoClose(file);
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_PACK_ID;
    }

    body_size =
        pack->badge_count *
        PACH_BADGE_RECORD_SIZE;

    result = pach_badge_read_exact(
        file,
        record_data,
        body_size
    );

    if (result >= 0) {
        result = pach_badge_read_exact(
            file,
            (u8 *)pack->pixels,
            pack->pixel_data_size
        );
    }

    sceIoClose(file);

    if (result < 0) {
        pach_badge_reset(pack);
        return result;
    }

    for (index = 0;
         index < pack->badge_count;
         ++index) {

        const u8 *record =
            &record_data[
                index * PACH_BADGE_RECORD_SIZE
            ];

        PachBadgeRecord *destination =
            &pack->records[index];

        destination->achievement_id =
            pach_badge_read_u32_le(&record[0]);

        destination->pixel_offset =
            pach_badge_read_u32_le(&record[4]);

        destination->pixel_size =
            pach_badge_read_u32_le(&record[8]);

        if (destination->achievement_id == 0 ||
            destination->pixel_size !=
                PACH_BADGE_PIXELS_PER_IMAGE * 2u ||
            destination->pixel_offset !=
                index *
                PACH_BADGE_PIXELS_PER_IMAGE * 2u ||
            destination->pixel_offset +
                destination->pixel_size >
                pack->pixel_data_size) {

            pach_badge_reset(pack);
            return PACH_BADGE_ERROR_RECORD;
        }

        if (pach_badge_has_duplicate_id(
                pack,
                index,
                destination->achievement_id)) {

            pach_badge_reset(pack);
            return PACH_BADGE_ERROR_DUPLICATE;
        }
    }

    /* The checksum field is treated as zero while hashing. */
    header[40] = 0;
    header[41] = 0;
    header[42] = 0;
    header[43] = 0;

    calculated_checksum =
        pach_badge_checksum_update(
            PACH_BADGE_FNV_OFFSET,
            header,
            sizeof(header)
        );

    calculated_checksum =
        pach_badge_checksum_update(
            calculated_checksum,
            record_data,
            body_size
        );

    calculated_checksum =
        pach_badge_checksum_update(
            calculated_checksum,
            (const u8 *)pack->pixels,
            pack->pixel_data_size
        );

    if (calculated_checksum != stored_checksum) {
        pach_badge_reset(pack);
        return PACH_BADGE_ERROR_CHECKSUM;
    }

    pack->loaded = 1;
    return PACH_BADGE_OK;
}

const u16 *pach_badge_get_pixels(
    const PachBadgePack *pack,
    u32 achievement_id)
{
    u32 index;

    if (pack == NULL ||
        !pack->loaded ||
        achievement_id == 0) {

        return NULL;
    }

    for (index = 0;
         index < pack->badge_count;
         ++index) {

        const PachBadgeRecord *record =
            &pack->records[index];

        if (record->achievement_id ==
            achievement_id) {

            return &pack->pixels[
                record->pixel_offset / 2u
            ];
        }
    }

    return NULL;
}
