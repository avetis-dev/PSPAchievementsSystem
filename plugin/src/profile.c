#include "pach_profile.h"

#include <stddef.h>

#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>

#define PACH_PROFILE_BASE_DIRECTORY \
    "ms0:/SEPLUGINS/PSPAchievementsNG"

#define PACH_PROFILE_DIRECTORY \
    PACH_PROFILE_BASE_DIRECTORY "/profiles"

#define PACH_PROFILE_DIRECTORY_PREFIX \
    PACH_PROFILE_DIRECTORY "/"

#define PACH_PROFILE_MAIN_EXTENSION   ".dat"
#define PACH_PROFILE_TEMP_EXTENSION   ".tmp"
#define PACH_PROFILE_BACKUP_EXTENSION ".bak"

#define PACH_PROFILE_FNV_OFFSET 2166136261u
#define PACH_PROFILE_FNV_PRIME  16777619u

static u16 pach_profile_read_u16_le(
    const u8 *data)
{
    return
        (u16)data[0] |
        ((u16)data[1] << 8);
}

static u32 pach_profile_read_u32_le(
    const u8 *data)
{
    return
        (u32)data[0] |
        ((u32)data[1] << 8) |
        ((u32)data[2] << 16) |
        ((u32)data[3] << 24);
}

static void pach_profile_write_u16_le(
    u8 *data,
    u16 value)
{
    data[0] = (u8)(value & 0xFFu);
    data[1] = (u8)((value >> 8) & 0xFFu);
}

static void pach_profile_write_u32_le(
    u8 *data,
    u32 value)
{
    data[0] = (u8)(value & 0xFFu);
    data[1] = (u8)((value >> 8) & 0xFFu);
    data[2] = (u8)((value >> 16) & 0xFFu);
    data[3] = (u8)((value >> 24) & 0xFFu);
}

static int pach_profile_validate_game_id(
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

static int pach_profile_game_ids_equal(
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

static int pach_profile_append_text(
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

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    while (text[index] != '\0') {
        if (*position + 1 >= output_capacity) {
            return PACH_PROFILE_ERROR_PATH;
        }

        output[*position] = text[index];

        ++(*position);
        ++index;
    }

    output[*position] = '\0';

    return PACH_PROFILE_OK;
}

static int pach_profile_build_one_path(
    const char *game_id,
    const char *extension,
    char *output,
    u32 output_capacity)
{
    u32 position = 0;
    int result;

    if (game_id == NULL ||
        extension == NULL ||
        output == NULL) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    output[0] = '\0';

    /*
     * Для пути файла используется вариант каталога
     * с завершающим слешем:
     *
     * .../profiles/ULUS-10285.dat
     */
    result = pach_profile_append_text(
        output,
        output_capacity,
        &position,
        PACH_PROFILE_DIRECTORY_PREFIX
    );

    if (result < 0) {
        return result;
    }

    result = pach_profile_append_text(
        output,
        output_capacity,
        &position,
        game_id
    );

    if (result < 0) {
        return result;
    }

    return pach_profile_append_text(
        output,
        output_capacity,
        &position,
        extension
    );
}

static u32 pach_profile_checksum_update(
    u32 checksum,
    const u8 *data,
    u32 size)
{
    u32 index;

    for (index = 0; index < size; ++index) {
        checksum ^= (u32)data[index];
        checksum *= PACH_PROFILE_FNV_PRIME;
    }

    return checksum;
}

static u32 pach_profile_calculate_checksum(
    const u8 header[PACH_PROFILE_HEADER_SIZE],
    const u8 *id_data,
    u32 id_data_size)
{
    u32 checksum = PACH_PROFILE_FNV_OFFSET;

    /*
     * Первые 28 байт заголовка входят в checksum.
     * Последние четыре байта содержат сам checksum.
     */
    checksum = pach_profile_checksum_update(
        checksum,
        header,
        28u
    );

    if (id_data != NULL &&
        id_data_size > 0) {

        checksum = pach_profile_checksum_update(
            checksum,
            id_data,
            id_data_size
        );
    }

    return checksum;
}

static int pach_profile_write_all(
    SceUID file,
    const u8 *data,
    u32 size)
{
    u32 total_written = 0;

    if (file < 0 || data == NULL) {
        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    while (total_written < size) {
        int written = sceIoWrite(
            file,
            data + total_written,
            size - total_written
        );

        if (written <= 0) {
            return PACH_PROFILE_ERROR_WRITE;
        }

        total_written += (u32)written;
    }

    return PACH_PROFILE_OK;
}

static int pach_profile_contains_id(
    const PachProfile *profile,
    u32 achievement_id)
{
    u32 index;

    if (profile == NULL ||
        achievement_id == 0) {

        return 0;
    }

    for (index = 0;
         index < profile->unlocked_count;
         ++index) {

        if (profile->unlocked_ids[index] ==
            achievement_id) {

            return 1;
        }
    }

    return 0;
}

void pach_profile_reset(
    PachProfile *profile)
{
    u32 index;

    if (profile == NULL) {
        return;
    }

    for (index = 0;
         index < PACH_GAME_ID_CAPACITY;
         ++index) {

        profile->game_id[index] = '\0';
    }

    profile->package_id = 0;
    profile->unlocked_count = 0;

    for (index = 0;
         index < PACH_ACHIEVEMENT_MAX;
         ++index) {

        profile->unlocked_ids[index] = 0;
    }

    profile->loaded = 0;
    profile->dirty = 0;
    profile->recovered_from_backup = 0;
    profile->primary_error = 0;
}

int pach_profile_build_paths(
    const char *game_id,
    PachProfilePaths *paths)
{
    int result;

    if (game_id == NULL ||
        paths == NULL) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    if (!pach_profile_validate_game_id(game_id)) {
        return PACH_PROFILE_ERROR_GAME_ID;
    }

    result = pach_profile_build_one_path(
        game_id,
        PACH_PROFILE_MAIN_EXTENSION,
        paths->main_path,
        sizeof(paths->main_path)
    );

    if (result < 0) {
        return result;
    }

    result = pach_profile_build_one_path(
        game_id,
        PACH_PROFILE_TEMP_EXTENSION,
        paths->temp_path,
        sizeof(paths->temp_path)
    );

    if (result < 0) {
        return result;
    }

    return pach_profile_build_one_path(
        game_id,
        PACH_PROFILE_BACKUP_EXTENSION,
        paths->backup_path,
        sizeof(paths->backup_path)
    );
}

int pach_profile_init_new(
    PachProfile *profile,
    const char *game_id,
    u32 package_id)
{
    int index;

    if (profile == NULL ||
        game_id == NULL ||
        package_id == 0) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    if (!pach_profile_validate_game_id(game_id)) {
        return PACH_PROFILE_ERROR_GAME_ID;
    }

    pach_profile_reset(profile);

    for (index = 0;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        profile->game_id[index] =
            game_id[index];
    }

    profile->game_id[
        PACH_GAME_ID_LENGTH
    ] = '\0';

    profile->package_id = package_id;
    profile->loaded = 1;
    profile->dirty = 1;

    return PACH_PROFILE_OK;
}

static int pach_profile_load_one(
    const char *path,
    const char *expected_game_id,
    u32 expected_package_id,
    PachProfile *profile)
{
    u8 header[PACH_PROFILE_HEADER_SIZE];

    u8 id_data[
        PACH_ACHIEVEMENT_MAX * 4u
    ];

    SceIoStat stat;
    SceUID file;

    u32 stored_checksum;
    u32 calculated_checksum;
    u32 id_data_size;
    u32 index;

    int bytes_read;
    int package_mismatch = 0;

    if (path == NULL ||
        expected_game_id == NULL ||
        profile == NULL ||
        expected_package_id == 0) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    pach_profile_reset(profile);

    if (!pach_profile_validate_game_id(
            expected_game_id)) {

        return PACH_PROFILE_ERROR_GAME_ID;
    }

    if (sceIoGetstat(
            path,
            &stat) < 0) {

        return PACH_PROFILE_NOT_FOUND;
    }

    file = sceIoOpen(
        path,
        PSP_O_RDONLY,
        0777
    );

    if (file < 0) {
        return PACH_PROFILE_ERROR_OPEN;
    }

    bytes_read = sceIoRead(
        file,
        header,
        sizeof(header)
    );

    if (bytes_read !=
        (int)PACH_PROFILE_HEADER_SIZE) {

        sceIoClose(file);
        return PACH_PROFILE_ERROR_READ;
    }

    if (header[0] != 'P' ||
        header[1] != 'P' ||
        header[2] != 'R' ||
        header[3] != 'F') {

        sceIoClose(file);
        return PACH_PROFILE_ERROR_MAGIC;
    }

    if (pach_profile_read_u16_le(
            &header[4]) !=
        PACH_PROFILE_FORMAT_VERSION) {

        sceIoClose(file);
        return PACH_PROFILE_ERROR_VERSION;
    }

    if (pach_profile_read_u16_le(
            &header[6]) !=
        PACH_PROFILE_HEADER_SIZE) {

        sceIoClose(file);
        return PACH_PROFILE_ERROR_HEADER_SIZE;
    }

    for (index = 0;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        profile->game_id[index] =
            (char)header[8 + index];
    }

    profile->game_id[
        PACH_GAME_ID_LENGTH
    ] = '\0';

    if (header[18] != 0 ||
        header[19] != 0 ||
        !pach_profile_validate_game_id(
            profile->game_id)) {

        sceIoClose(file);
        pach_profile_reset(profile);

        return PACH_PROFILE_ERROR_GAME_ID;
    }

    if (!pach_profile_game_ids_equal(
            profile->game_id,
            expected_game_id)) {

        sceIoClose(file);
        pach_profile_reset(profile);

        return PACH_PROFILE_ERROR_GAME_MISMATCH;
    }

    profile->package_id =
        pach_profile_read_u32_le(
            &header[20]
        );

    profile->unlocked_count =
        pach_profile_read_u32_le(
            &header[24]
        );

    stored_checksum =
        pach_profile_read_u32_le(
            &header[28]
        );

    if (profile->package_id != expected_package_id) {
        package_mismatch = 1;
    }

    if (profile->unlocked_count >
        PACH_ACHIEVEMENT_MAX) {

        sceIoClose(file);
        pach_profile_reset(profile);

        return PACH_PROFILE_ERROR_COUNT;
    }

    id_data_size =
        profile->unlocked_count * 4u;

    if (id_data_size > 0) {
        bytes_read = sceIoRead(
            file,
            id_data,
            id_data_size
        );

        if (bytes_read !=
            (int)id_data_size) {

            sceIoClose(file);
            pach_profile_reset(profile);

            return PACH_PROFILE_ERROR_READ;
        }
    }

    sceIoClose(file);

    calculated_checksum =
        pach_profile_calculate_checksum(
            header,
            id_data,
            id_data_size
        );

    if (calculated_checksum !=
        stored_checksum) {

        pach_profile_reset(profile);

        return PACH_PROFILE_ERROR_CHECKSUM;
    }

    /*
     * unlocked_count уже известен из заголовка.
     * Проверяем каждый ID только относительно ранее
     * прочитанных элементов, чтобы корректно выявить
     * дубликаты.
     */
    for (index = 0;
         index < profile->unlocked_count;
         ++index) {

        u32 previous_index;

        u32 achievement_id =
            pach_profile_read_u32_le(
                &id_data[index * 4u]
            );

        if (achievement_id == 0) {
            pach_profile_reset(profile);

            return PACH_PROFILE_ERROR_DUPLICATE_ID;
        }

        for (previous_index = 0;
             previous_index < index;
             ++previous_index) {

            if (profile->unlocked_ids[
                    previous_index
                ] == achievement_id) {

                pach_profile_reset(profile);

                return
                    PACH_PROFILE_ERROR_DUPLICATE_ID;
            }
        }

        profile->unlocked_ids[index] =
            achievement_id;
    }

    profile->loaded = 1;
    profile->dirty = 0;

    if (package_mismatch) {
        return PACH_PROFILE_PACKAGE_MISMATCH;
    }

    return PACH_PROFILE_OK;
}

int pach_profile_load(
    const PachProfilePaths *paths,
    const char *expected_game_id,
    u32 expected_package_id,
    PachProfile *profile)
{
    PachProfile loaded_profile;
    int main_result;
    int backup_result;

    if (paths == NULL ||
        expected_game_id == NULL ||
        profile == NULL ||
        expected_package_id == 0) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    pach_profile_reset(profile);
    pach_profile_reset(&loaded_profile);

    main_result = pach_profile_load_one(
        paths->main_path,
        expected_game_id,
        expected_package_id,
        &loaded_profile
    );

    if (main_result == PACH_PROFILE_OK ||
        main_result == PACH_PROFILE_PACKAGE_MISMATCH) {

        *profile = loaded_profile;
        return main_result;
    }

    pach_profile_reset(&loaded_profile);

    backup_result = pach_profile_load_one(
        paths->backup_path,
        expected_game_id,
        expected_package_id,
        &loaded_profile
    );

    if (backup_result == PACH_PROFILE_OK ||
        backup_result == PACH_PROFILE_PACKAGE_MISMATCH) {

        loaded_profile.recovered_from_backup = 1;
        loaded_profile.primary_error = main_result;
        *profile = loaded_profile;
        return backup_result;
    }

    pach_profile_reset(profile);

    if (main_result != PACH_PROFILE_NOT_FOUND) {
        return main_result;
    }

    if (backup_result != PACH_PROFILE_NOT_FOUND) {
        return backup_result;
    }

    return PACH_PROFILE_NOT_FOUND;
}

int pach_profile_add_unlocked(
    PachProfile *profile,
    u32 achievement_id)
{
    if (profile == NULL ||
        !profile->loaded ||
        achievement_id == 0) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    if (pach_profile_contains_id(
            profile,
            achievement_id)) {

        return PACH_PROFILE_EXISTS;
    }

    if (profile->unlocked_count >=
        PACH_ACHIEVEMENT_MAX) {

        return PACH_PROFILE_ERROR_COUNT;
    }

    profile->unlocked_ids[
        profile->unlocked_count
    ] = achievement_id;

    ++profile->unlocked_count;

    profile->dirty = 1;

    return PACH_PROFILE_ADDED;
}

int pach_profile_save_atomic(
    const PachProfilePaths *paths,
    PachProfile *profile)
{
    u8 header[PACH_PROFILE_HEADER_SIZE];

    u8 id_data[
        PACH_ACHIEVEMENT_MAX * 4u
    ];

    SceUID file;
    PachProfile verified_profile;

    u32 id_data_size;
    u32 checksum;
    u32 index;

    int result;
    int had_old_profile = 0;

    if (paths == NULL ||
        profile == NULL ||
        !profile->loaded ||
        profile->package_id == 0 ||
        profile->unlocked_count >
            PACH_ACHIEVEMENT_MAX) {

        return PACH_PROFILE_ERROR_ARGUMENT;
    }

    if (!pach_profile_validate_game_id(
            profile->game_id)) {

        return PACH_PROFILE_ERROR_GAME_ID;
    }

    for (index = 0;
         index < PACH_PROFILE_HEADER_SIZE;
         ++index) {

        header[index] = 0;
    }

    for (index = 0;
         index < sizeof(id_data);
         ++index) {

        id_data[index] = 0;
    }

    header[0] = 'P';
    header[1] = 'P';
    header[2] = 'R';
    header[3] = 'F';

    pach_profile_write_u16_le(
        &header[4],
        PACH_PROFILE_FORMAT_VERSION
    );

    pach_profile_write_u16_le(
        &header[6],
        PACH_PROFILE_HEADER_SIZE
    );

    for (index = 0;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        header[8 + index] =
            (u8)profile->game_id[index];
    }

    pach_profile_write_u32_le(
        &header[20],
        profile->package_id
    );

    pach_profile_write_u32_le(
        &header[24],
        profile->unlocked_count
    );

    id_data_size =
        profile->unlocked_count * 4u;

    for (index = 0;
         index < profile->unlocked_count;
         ++index) {

        pach_profile_write_u32_le(
            &id_data[index * 4u],
            profile->unlocked_ids[index]
        );
    }

    checksum =
        pach_profile_calculate_checksum(
            header,
            id_data,
            id_data_size
        );

    pach_profile_write_u32_le(
        &header[28],
        checksum
    );

    /*
     * Для sceIoMkdir путь передаётся без
     * завершающего символа '/'.
     */
    sceIoMkdir(
        PACH_PROFILE_BASE_DIRECTORY,
        0777
    );

    sceIoMkdir(
        PACH_PROFILE_DIRECTORY,
        0777
    );

    sceIoRemove(
        paths->temp_path
    );

    file = sceIoOpen(
        paths->temp_path,
        PSP_O_WRONLY |
        PSP_O_CREAT |
        PSP_O_TRUNC,
        0777
    );

    if (file < 0) {
        return PACH_PROFILE_ERROR_OPEN;
    }

    result = pach_profile_write_all(
        file,
        header,
        sizeof(header)
    );

    if (result >= 0 &&
        id_data_size > 0) {

        result = pach_profile_write_all(
            file,
            id_data,
            id_data_size
        );
    }

    sceIoClose(file);

    if (result < 0) {
        sceIoRemove(
            paths->temp_path
        );

        return result;
    }

    sceIoSync(
        "ms0:",
        0
    );

    /*
     * Verify the complete temporary file before replacing either copy.
     * This catches short writes and checksum corruption while the previous
     * profile is still intact.
     */
    pach_profile_reset(&verified_profile);

    result = pach_profile_load_one(
        paths->temp_path,
        profile->game_id,
        profile->package_id,
        &verified_profile
    );

    if (result != PACH_PROFILE_OK ||
        verified_profile.unlocked_count !=
            profile->unlocked_count) {

        sceIoRemove(paths->temp_path);
        return PACH_PROFILE_ERROR_VERIFY;
    }

    if (profile->recovered_from_backup) {
        /*
         * The main file is missing or corrupt. Keep the known-good backup
         * untouched and replace only the broken primary copy.
         */
        sceIoRemove(paths->main_path);
    } else {
        SceIoStat stat;

        if (sceIoGetstat(paths->main_path, &stat) >= 0) {
            sceIoRemove(paths->backup_path);

            if (sceIoRename(
                    paths->main_path,
                    paths->backup_path) < 0) {

                sceIoRemove(paths->temp_path);
                return PACH_PROFILE_ERROR_RENAME;
            }

            had_old_profile = 1;
        }
    }

    if (sceIoRename(
            paths->temp_path,
            paths->main_path) < 0) {

        sceIoRemove(paths->temp_path);

        if (had_old_profile) {
            (void)sceIoRename(
                paths->backup_path,
                paths->main_path
            );
        }

        return PACH_PROFILE_ERROR_RENAME;
    }

    /*
     * Deliberately keep .bak. It is the previous verified generation and
     * is used automatically if the main profile is ever damaged.
     */
    sceIoSync(
        "ms0:",
        0
    );

    profile->dirty = 0;
    profile->recovered_from_backup = 0;
    profile->primary_error = 0;

    return PACH_PROFILE_OK;
}