#include "pach_package.h"

#include <stddef.h>

#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>

#define PACH_PACKAGE_DIRECTORY \
    "ms0:/SEPLUGINS/PSPAchievementsNG/games/"

#define PACH_PACKAGE_EXTENSION ".pach"
#define PACH_PACKAGE_FNV_OFFSET 2166136261u
#define PACH_PACKAGE_FNV_PRIME  16777619u

static u16 pach_package_read_u16_le(const u8 *data)
{
    return (u16)data[0] | ((u16)data[1] << 8);
}

static u32 pach_package_read_u32_le(const u8 *data)
{
    return
        (u32)data[0] |
        ((u32)data[1] << 8) |
        ((u32)data[2] << 16) |
        ((u32)data[3] << 24);
}

static u32 pach_package_checksum_update(
    u32 checksum,
    const u8 *data,
    u32 size)
{
    u32 index;

    for (index = 0; index < size; ++index) {
        checksum ^= (u32)data[index];
        checksum *= PACH_PACKAGE_FNV_PRIME;
    }

    return checksum;
}

static int pach_package_read_exact(
    SceUID file,
    u8 *data,
    u32 size)
{
    u32 total = 0;

    if (file < 0 || data == NULL) {
        return PACH_PACKAGE_ERROR_ARGUMENT;
    }

    while (total < size) {
        int count = sceIoRead(file, data + total, size - total);

        if (count <= 0) {
            return PACH_PACKAGE_ERROR_READ;
        }

        total += (u32)count;
    }

    return PACH_PACKAGE_OK;
}

static int pach_package_validate_game_id(const char *game_id)
{
    int index;

    if (game_id == NULL) {
        return 0;
    }

    for (index = 0; index < 4; ++index) {
        if (game_id[index] < 'A' || game_id[index] > 'Z') {
            return 0;
        }
    }

    if (game_id[4] != '-') {
        return 0;
    }

    for (index = 5; index < PACH_GAME_ID_LENGTH; ++index) {
        if (game_id[index] < '0' || game_id[index] > '9') {
            return 0;
        }
    }

    return game_id[PACH_GAME_ID_LENGTH] == '\0';
}

static int pach_package_game_ids_equal(
    const char *left,
    const char *right)
{
    int index;

    if (left == NULL || right == NULL) {
        return 0;
    }

    for (index = 0; index < PACH_GAME_ID_LENGTH; ++index) {
        if (left[index] != right[index]) {
            return 0;
        }
    }

    return
        left[PACH_GAME_ID_LENGTH] == '\0' &&
        right[PACH_GAME_ID_LENGTH] == '\0';
}

static int pach_package_append_text(
    char *output,
    u32 output_capacity,
    u32 *position,
    const char *text)
{
    u32 index = 0;

    if (output == NULL || position == NULL ||
        text == NULL || output_capacity == 0) {

        return PACH_PACKAGE_ERROR_ARGUMENT;
    }

    while (text[index] != '\0') {
        if (*position + 1u >= output_capacity) {
            return PACH_PACKAGE_ERROR_PATH;
        }

        output[*position] = text[index];
        ++(*position);
        ++index;
    }

    output[*position] = '\0';
    return PACH_PACKAGE_OK;
}

static void pach_package_reset_operand(PachOperand *operand)
{
    if (operand == NULL) {
        return;
    }

    operand->type = PACH_OPERAND_VALUE_U32;
    operand->state = PACH_OPERAND_STATE_CURRENT;
    operand->memory_index = PACH_OPERAND_MEMORY_INDEX_NONE;
    operand->value = 0;
}

static int pach_package_operand_is_memory(u8 type)
{
    return
        type >= PACH_OPERAND_MEMORY_U8 &&
        type <= PACH_OPERAND_MEMORY_BITCOUNT;
}

static int pach_package_operand_valid(const PachOperand *operand)
{
    int memory_operand;

    if (operand == NULL ||
        operand->type > PACH_OPERAND_MEMORY_BITCOUNT ||
        operand->state > PACH_OPERAND_STATE_PRIOR) {

        return 0;
    }

    memory_operand = pach_package_operand_is_memory(
        operand->type
    );

    if (!memory_operand &&
        operand->state != PACH_OPERAND_STATE_CURRENT) {

        return 0;
    }

    if (!memory_operand &&
        operand->memory_index != PACH_OPERAND_MEMORY_INDEX_NONE) {

        return 0;
    }

    if (memory_operand &&
        operand->memory_index != PACH_OPERAND_MEMORY_INDEX_NONE) {

        if (operand->memory_index < PACH_OPERAND_DIVISOR_MIN ||
            operand->memory_index > PACH_OPERAND_DIVISOR_MAX ||
            operand->type == PACH_OPERAND_MEMORY_FLOAT) {

            return 0;
        }
    }

    return 1;
}

static int pach_package_has_duplicate_id(
    const PachPackageInfo *package_info,
    u32 current_index,
    u32 achievement_id)
{
    u32 index;

    for (index = 0; index < current_index; ++index) {
        if (package_info->achievements[index].id == achievement_id) {
            return 1;
        }
    }

    return 0;
}

static int pach_package_string_valid(
    const PachPackageInfo *package_info,
    u32 offset)
{
    u32 index;

    if (package_info == NULL ||
        offset >= package_info->string_table_size) {

        return 0;
    }

    for (index = offset;
         index < package_info->string_table_size;
         ++index) {

        if (package_info->string_table[index] == '\0') {
            return 1;
        }
    }

    return 0;
}

void pach_package_reset(PachPackageInfo *package_info)
{
    u32 index;

    if (package_info == NULL) {
        return;
    }

    package_info->format_version = 0;
    package_info->header_size = 0;
    package_info->achievement_count = 0;
    package_info->group_count = 0;
    package_info->condition_count = 0;
    package_info->string_table_size = 0;
    package_info->flags = 0;
    package_info->package_id = 0;
    package_info->loaded = 0;

    for (index = 0; index < PACH_GAME_ID_CAPACITY; ++index) {
        package_info->game_id[index] = '\0';
    }

    for (index = 0; index < PACH_ACHIEVEMENT_MAX; ++index) {
        PachAchievementDefinition *definition =
            &package_info->achievements[index];

        definition->id = 0;
        definition->points = 0;
        definition->type = 0;
        definition->flags = 0;
        definition->first_group = 0;
        definition->group_count = 0;
        definition->title_offset = 0;
        definition->description_offset = 0;
        definition->badge_id = 0;
        definition->author_offset = 0;
        definition->reserved = 0;
    }

    for (index = 0; index < PACH_ACHIEVEMENT_GROUP_MAX; ++index) {
        package_info->groups[index].first_condition = 0;
        package_info->groups[index].condition_count = 0;
        package_info->groups[index].flags = 0;
    }

    for (index = 0; index < PACH_ACHIEVEMENT_CONDITION_MAX; ++index) {
        pach_package_reset_operand(
            &package_info->conditions[index].left
        );
        pach_package_reset_operand(
            &package_info->conditions[index].right
        );
        package_info->conditions[index].comparison = PACH_COMPARE_EQUAL;
        package_info->conditions[index].flags = 0;
        package_info->conditions[index].hit_target = 0;
    }

    package_info->string_table[0] = '\0';
}

int pach_package_build_path(
    const char *game_id,
    char *output,
    u32 output_capacity)
{
    u32 position = 0;
    int result;

    if (game_id == NULL || output == NULL || output_capacity == 0) {
        return PACH_PACKAGE_ERROR_ARGUMENT;
    }

    if (!pach_package_validate_game_id(game_id)) {
        return PACH_PACKAGE_ERROR_GAME_ID;
    }

    output[0] = '\0';

    result = pach_package_append_text(
        output,
        output_capacity,
        &position,
        PACH_PACKAGE_DIRECTORY
    );

    if (result < 0) {
        return result;
    }

    result = pach_package_append_text(
        output,
        output_capacity,
        &position,
        game_id
    );

    if (result < 0) {
        return result;
    }

    return pach_package_append_text(
        output,
        output_capacity,
        &position,
        PACH_PACKAGE_EXTENSION
    );
}

const char *pach_package_get_string(
    const PachPackageInfo *package_info,
    u32 offset)
{
    if (!pach_package_string_valid(package_info, offset)) {
        return NULL;
    }

    return &package_info->string_table[offset];
}

int pach_package_contains_achievement_id(
    const PachPackageInfo *package_info,
    u32 achievement_id)
{
    u32 index;

    if (package_info == NULL || !package_info->loaded ||
        achievement_id == 0) {

        return 0;
    }

    for (index = 0; index < package_info->achievement_count; ++index) {
        if (package_info->achievements[index].id == achievement_id) {
            return 1;
        }
    }

    return 0;
}

int pach_package_load(
    const char *path,
    const char *expected_game_id,
    PachPackageInfo *package_info)
{
    u8 header[PACH_PACKAGE_HEADER_SIZE];
    u8 record[PACH_PACKAGE_ACHIEVEMENT_SIZE];
    u8 group_data[PACH_PACKAGE_GROUP_SIZE];
    u8 condition_data[PACH_PACKAGE_CONDITION_SIZE];
    SceUID file;
    u32 checksum = PACH_PACKAGE_FNV_OFFSET;
    u32 index;
    int result;

    if (path == NULL || expected_game_id == NULL ||
        package_info == NULL) {

        return PACH_PACKAGE_ERROR_ARGUMENT;
    }

    pach_package_reset(package_info);

    if (!pach_package_validate_game_id(expected_game_id)) {
        return PACH_PACKAGE_ERROR_GAME_ID;
    }

    file = sceIoOpen(path, PSP_O_RDONLY, 0777);

    if (file < 0) {
        return file;
    }

    result = pach_package_read_exact(file, header, sizeof(header));

    if (result < 0) {
        sceIoClose(file);
        return result;
    }

    if (header[0] != 'P' || header[1] != 'A' ||
        header[2] != 'C' || header[3] != 'H') {

        sceIoClose(file);
        return PACH_PACKAGE_ERROR_MAGIC;
    }

    package_info->format_version = pach_package_read_u16_le(&header[4]);
    package_info->header_size = pach_package_read_u16_le(&header[6]);

    if (package_info->format_version != PACH_PACKAGE_FORMAT_VERSION) {
        sceIoClose(file);
        return PACH_PACKAGE_ERROR_VERSION;
    }

    if (package_info->header_size != PACH_PACKAGE_HEADER_SIZE) {
        sceIoClose(file);
        return PACH_PACKAGE_ERROR_HEADER_SIZE;
    }

    for (index = 0; index < PACH_GAME_ID_LENGTH; ++index) {
        package_info->game_id[index] = (char)header[8 + index];
    }
    package_info->game_id[PACH_GAME_ID_LENGTH] = '\0';

    if (header[18] != 0 || header[19] != 0 ||
        !pach_package_validate_game_id(package_info->game_id)) {

        sceIoClose(file);
        return PACH_PACKAGE_ERROR_GAME_ID;
    }

    if (!pach_package_game_ids_equal(
            package_info->game_id,
            expected_game_id)) {

        sceIoClose(file);
        return PACH_PACKAGE_ERROR_GAME_MISMATCH;
    }

    package_info->achievement_count = pach_package_read_u32_le(&header[20]);
    package_info->group_count = pach_package_read_u32_le(&header[24]);
    package_info->condition_count = pach_package_read_u32_le(&header[28]);
    package_info->string_table_size = pach_package_read_u32_le(&header[32]);
    package_info->flags = pach_package_read_u32_le(&header[36]);
    package_info->package_id = pach_package_read_u32_le(&header[40]);

    if (pach_package_read_u32_le(&header[44]) != 0 ||
        package_info->achievement_count > PACH_ACHIEVEMENT_MAX ||
        package_info->group_count > PACH_ACHIEVEMENT_GROUP_MAX ||
        package_info->condition_count > PACH_ACHIEVEMENT_CONDITION_MAX ||
        package_info->string_table_size == 0 ||
        package_info->string_table_size > PACH_ACHIEVEMENT_STRING_MAX ||
        (package_info->achievement_count == 0 &&
         (package_info->group_count != 0 ||
          package_info->condition_count != 0)) ||
        (package_info->achievement_count > 0 &&
         package_info->group_count == 0)) {

        sceIoClose(file);
        return PACH_PACKAGE_ERROR_COUNT;
    }

    if (package_info->package_id == 0) {
        sceIoClose(file);
        return PACH_PACKAGE_ERROR_PACKAGE_ID;
    }

    for (index = 0; index < package_info->achievement_count; ++index) {
        PachAchievementDefinition *definition =
            &package_info->achievements[index];

        result = pach_package_read_exact(file, record, sizeof(record));
        if (result < 0) {
            sceIoClose(file);
            return result;
        }

        checksum = pach_package_checksum_update(
            checksum,
            record,
            sizeof(record)
        );

        definition->id = pach_package_read_u32_le(&record[0]);
        definition->points = pach_package_read_u16_le(&record[4]);
        definition->type = record[6];
        definition->flags = record[7];
        definition->first_group = pach_package_read_u16_le(&record[8]);
        definition->group_count = pach_package_read_u16_le(&record[10]);
        definition->title_offset = pach_package_read_u32_le(&record[12]);
        definition->description_offset = pach_package_read_u32_le(&record[16]);
        definition->badge_id = pach_package_read_u32_le(&record[20]);
        definition->author_offset = pach_package_read_u32_le(&record[24]);
        definition->reserved = pach_package_read_u32_le(&record[28]);

        if (definition->id == 0 || definition->group_count == 0 ||
            definition->first_group + definition->group_count >
                package_info->group_count ||
            definition->flags != 0 || definition->reserved != 0) {

            sceIoClose(file);
            return PACH_PACKAGE_ERROR_ACHIEVEMENT;
        }

        if (pach_package_has_duplicate_id(
                package_info,
                index,
                definition->id)) {

            sceIoClose(file);
            return PACH_PACKAGE_ERROR_DUPLICATE_ID;
        }
    }

    for (index = 0; index < package_info->group_count; ++index) {
        PachAchievementGroup *group = &package_info->groups[index];

        result = pach_package_read_exact(
            file,
            group_data,
            sizeof(group_data)
        );
        if (result < 0) {
            sceIoClose(file);
            return result;
        }

        checksum = pach_package_checksum_update(
            checksum,
            group_data,
            sizeof(group_data)
        );

        group->first_condition = pach_package_read_u32_le(&group_data[0]);
        group->condition_count = pach_package_read_u16_le(&group_data[4]);
        group->flags = pach_package_read_u16_le(&group_data[6]);

        if (group->first_condition + group->condition_count >
                package_info->condition_count ||
            group->flags != 0) {

            sceIoClose(file);
            return PACH_PACKAGE_ERROR_GROUP;
        }
    }

    for (index = 0; index < package_info->condition_count; ++index) {
        PachAchievementCondition *condition =
            &package_info->conditions[index];

        result = pach_package_read_exact(
            file,
            condition_data,
            sizeof(condition_data)
        );
        if (result < 0) {
            sceIoClose(file);
            return result;
        }

        checksum = pach_package_checksum_update(
            checksum,
            condition_data,
            sizeof(condition_data)
        );

        condition->left.type = condition_data[0];
        condition->left.state = condition_data[1];
        condition->left.memory_index =
            pach_package_read_u16_le(&condition_data[2]);
        condition->left.value = pach_package_read_u32_le(&condition_data[4]);

        condition->right.type = condition_data[8];
        condition->right.state = condition_data[9];
        condition->right.memory_index =
            pach_package_read_u16_le(&condition_data[10]);
        condition->right.value = pach_package_read_u32_le(&condition_data[12]);

        condition->comparison = condition_data[16];
        condition->flags = condition_data[17];
        condition->hit_target = pach_package_read_u16_le(&condition_data[18]);

        if (!pach_package_operand_valid(&condition->left) ||
            !pach_package_operand_valid(&condition->right) ||
            condition->flags > PACH_CONDITION_SUB_HITS) {

            sceIoClose(file);
            return PACH_PACKAGE_ERROR_CONDITION;
        }

        if (condition->flags == PACH_CONDITION_ADD_SOURCE ||
            condition->flags == PACH_CONDITION_SUB_SOURCE ||
            condition->flags == PACH_CONDITION_ADD_ADDRESS) {

            if (condition->comparison != PACH_COMPARE_NONE ||
                condition->hit_target != 0) {

                sceIoClose(file);
                return PACH_PACKAGE_ERROR_CONDITION;
            }
        } else if (condition->comparison >
                   PACH_COMPARE_GREATER_EQUAL) {

            sceIoClose(file);
            return PACH_PACKAGE_ERROR_CONDITION;
        }
    }

    result = pach_package_read_exact(
        file,
        (u8 *)package_info->string_table,
        package_info->string_table_size
    );

    sceIoClose(file);

    if (result < 0) {
        return result;
    }

    checksum = pach_package_checksum_update(
        checksum,
        (const u8 *)package_info->string_table,
        package_info->string_table_size
    );

    if (checksum != package_info->package_id) {
        return PACH_PACKAGE_ERROR_CHECKSUM;
    }

    if (package_info->string_table[0] != '\0' ||
        package_info->string_table[
            package_info->string_table_size - 1u
        ] != '\0') {

        return PACH_PACKAGE_ERROR_STRING;
    }

    for (index = 0; index < package_info->achievement_count; ++index) {
        const PachAchievementDefinition *definition =
            &package_info->achievements[index];

        const char *title = pach_package_get_string(
            package_info,
            definition->title_offset
        );

        if (title == NULL || title[0] == '\0' ||
            pach_package_get_string(
                package_info,
                definition->description_offset) == NULL ||
            pach_package_get_string(
                package_info,
                definition->author_offset) == NULL) {

            return PACH_PACKAGE_ERROR_STRING;
        }
    }

    package_info->loaded = 1;
    return PACH_PACKAGE_OK;
}
