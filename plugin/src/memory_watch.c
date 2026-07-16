#include "pach_memory_watch.h"

static int pach_memory_watch_read(
    const PachMemoryWatch *watch,
    const PachMemoryMap *memory_map,
    u32 *value)
{
    int result;

    if (watch == NULL ||
        memory_map == NULL ||
        value == NULL) {

        return PACH_MEMORY_WATCH_ERROR_ARGUMENT;
    }

    *value = 0;

    switch (watch->width) {
    case PACH_MEMORY_WIDTH_8: {
        u8 value8 = 0;

        result = pach_memory_read_u8(
            memory_map,
            watch->address,
            watch->address_kind,
            &value8
        );

        if (result < 0) {
            return result;
        }

        *value = (u32)value8;

        return PACH_MEMORY_OK;
    }

    case PACH_MEMORY_WIDTH_16: {
        u16 value16 = 0;

        result = pach_memory_read_u16_le(
            memory_map,
            watch->address,
            watch->address_kind,
            &value16
        );

        if (result < 0) {
            return result;
        }

        *value = (u32)value16;

        return PACH_MEMORY_OK;
    }

    case PACH_MEMORY_WIDTH_24:
        return pach_memory_read_u24_le(
            memory_map,
            watch->address,
            watch->address_kind,
            value
        );

    case PACH_MEMORY_WIDTH_32:
        return pach_memory_read_u32_le(
            memory_map,
            watch->address,
            watch->address_kind,
            value
        );

    default:
        return PACH_MEMORY_WATCH_ERROR_WIDTH;
    }
}

void pach_memory_watch_reset(
    PachMemoryWatch *watch)
{
    if (watch == NULL) {
        return;
    }

    watch->address = 0;
    watch->address_kind = PACH_ADDRESS_OFFSET;
    watch->width = PACH_MEMORY_WIDTH_8;

    watch->current_value = 0;
    watch->previous_value = 0;
    watch->change_count = 0;

    watch->initialized = 0;
}

int pach_memory_watch_init(
    PachMemoryWatch *watch,
    u32 address,
    PachAddressKind address_kind,
    PachMemoryWidth width)
{
    if (watch == NULL) {
        return PACH_MEMORY_WATCH_ERROR_ARGUMENT;
    }

    if (width != PACH_MEMORY_WIDTH_8 &&
        width != PACH_MEMORY_WIDTH_16 &&
        width != PACH_MEMORY_WIDTH_24 &&
        width != PACH_MEMORY_WIDTH_32) {

        return PACH_MEMORY_WATCH_ERROR_WIDTH;
    }

    if (address_kind != PACH_ADDRESS_OFFSET &&
        address_kind != PACH_ADDRESS_USER &&
        address_kind != PACH_ADDRESS_KERNEL) {

        return PACH_MEMORY_ERROR_KIND;
    }

    pach_memory_watch_reset(watch);

    watch->address = address;
    watch->address_kind = address_kind;
    watch->width = width;

    return PACH_MEMORY_OK;
}

int pach_memory_watch_sample(
    PachMemoryWatch *watch,
    const PachMemoryMap *memory_map)
{
    u32 sampled_value;
    int result;

    if (watch == NULL ||
        memory_map == NULL) {

        return PACH_MEMORY_WATCH_ERROR_ARGUMENT;
    }

    result = pach_memory_watch_read(
        watch,
        memory_map,
        &sampled_value
    );

    if (result < 0) {
        return result;
    }

    /*
     * Первый sample только устанавливает начальное
     * значение. Изменением это не считается.
     */
    if (!watch->initialized) {
        watch->current_value = sampled_value;
        watch->previous_value = sampled_value;
        watch->change_count = 0;
        watch->initialized = 1;

        return PACH_MEMORY_WATCH_INITIALIZED;
    }

    watch->previous_value =
        watch->current_value;

    watch->current_value =
        sampled_value;

    if (watch->current_value !=
        watch->previous_value) {

        ++watch->change_count;

        return PACH_MEMORY_WATCH_CHANGED;
    }

    return PACH_MEMORY_WATCH_UNCHANGED;
}

int pach_memory_watch_rebase(
    PachMemoryWatch *watch)
{
    if (watch == NULL ||
        !watch->initialized) {

        return PACH_MEMORY_WATCH_ERROR_ARGUMENT;
    }

    /*
     * Текущее значение становится новой исходной
     * точкой. Это используется после завершения
     * загрузки executable игры.
     */
    watch->previous_value =
        watch->current_value;

    watch->change_count = 0;

    return PACH_MEMORY_OK;
}