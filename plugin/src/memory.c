#include "pach_memory.h"

static int pach_memory_get_offset(
    u32 source_address,
    PachAddressKind address_kind,
    u32 *offset)
{
    if (offset == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    switch (address_kind) {
    case PACH_ADDRESS_OFFSET:
        *offset = source_address;
        return PACH_MEMORY_OK;

    case PACH_ADDRESS_USER:
        if (source_address < PACH_USER_MEMORY_BASE) {
            return PACH_MEMORY_ERROR_RANGE;
        }

        *offset =
            source_address -
            PACH_USER_MEMORY_BASE;

        return PACH_MEMORY_OK;

    case PACH_ADDRESS_KERNEL:
        if (source_address < PACH_KERNEL_MEMORY_BASE) {
            return PACH_MEMORY_ERROR_RANGE;
        }

        *offset =
            source_address -
            PACH_KERNEL_MEMORY_BASE;

        return PACH_MEMORY_OK;

    default:
        return PACH_MEMORY_ERROR_KIND;
    }
}

void pach_memory_map_init(PachMemoryMap *map)
{
    if (map == NULL) {
        return;
    }

    map->ram_size = PACH_MEMORY_SIZE_32_MB;
    map->relocation = 0;
}

int pach_memory_translate(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 access_size,
    u32 *kernel_address)
{
    u32 source_offset;
    u32 adjusted_offset;

    SceInt64 adjusted_offset_64;

    int result;

    if (map == NULL || kernel_address == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *kernel_address = 0;

    if (map->ram_size == 0 || access_size == 0) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    result = pach_memory_get_offset(
        source_address,
        address_kind,
        &source_offset
    );

    if (result < 0) {
        return result;
    }

    adjusted_offset_64 =
        (SceInt64)source_offset +
        (SceInt64)map->relocation;

    if (adjusted_offset_64 < 0) {
        return PACH_MEMORY_ERROR_RANGE;
    }

    if (adjusted_offset_64 > 0xFFFFFFFFLL) {
        return PACH_MEMORY_ERROR_RANGE;
    }

    adjusted_offset = (u32)adjusted_offset_64;

    if (adjusted_offset >= map->ram_size) {
        return PACH_MEMORY_ERROR_RANGE;
    }

    /*
     * Исключаем переполнение выражения:
     *
     * adjusted_offset + access_size
     */
    if (access_size >
        map->ram_size - adjusted_offset) {

        return PACH_MEMORY_ERROR_RANGE;
    }

    *kernel_address =
        PACH_KERNEL_MEMORY_BASE +
        adjusted_offset;

    return PACH_MEMORY_OK;
}

int pach_memory_read_bytes(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u8 *buffer,
    u32 buffer_size)
{
    const volatile u8 *memory;

    u32 kernel_address;
    u32 index;

    int result;

    if (buffer == NULL || buffer_size == 0) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    result = pach_memory_translate(
        map,
        source_address,
        address_kind,
        buffer_size,
        &kernel_address
    );

    if (result < 0) {
        return result;
    }

    /*
     * Читаем побайтно.
     *
     * Это намеренно: прямое разыменование u16/u32
     * по невыравненному адресу на MIPS может вызвать
     * исключение Address Error.
     */
    memory =
        (const volatile u8 *)kernel_address;

    for (index = 0; index < buffer_size; ++index) {
        buffer[index] = memory[index];
    }

    return PACH_MEMORY_OK;
}

int pach_memory_read_u8(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u8 *value)
{
    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    return pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        value,
        1
    );
}

int pach_memory_read_u16_le(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u16 *value)
{
    u8 bytes[2];
    int result;

    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *value = 0;

    result = pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        bytes,
        sizeof(bytes)
    );

    if (result < 0) {
        return result;
    }

    *value =
        (u16)bytes[0] |
        ((u16)bytes[1] << 8);

    return PACH_MEMORY_OK;
}

int pach_memory_read_u24_le(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value)
{
    u8 bytes[3];
    int result;

    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *value = 0;

    result = pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        bytes,
        sizeof(bytes)
    );

    if (result < 0) {
        return result;
    }

    *value =
        (u32)bytes[0] |
        ((u32)bytes[1] << 8) |
        ((u32)bytes[2] << 16);

    return PACH_MEMORY_OK;
}

int pach_memory_read_u32_le(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value)
{
    u8 bytes[4];
    int result;

    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *value = 0;

    result = pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        bytes,
        sizeof(bytes)
    );

    if (result < 0) {
        return result;
    }

    *value =
        (u32)bytes[0] |
        ((u32)bytes[1] << 8) |
        ((u32)bytes[2] << 16) |
        ((u32)bytes[3] << 24);

    return PACH_MEMORY_OK;
}

int pach_memory_read_u16_be(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u16 *value)
{
    u8 bytes[2];
    int result;

    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *value = 0;

    result = pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        bytes,
        sizeof(bytes)
    );

    if (result < 0) {
        return result;
    }

    *value =
        ((u16)bytes[0] << 8) |
        (u16)bytes[1];

    return PACH_MEMORY_OK;
}

int pach_memory_read_u24_be(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value)
{
    u8 bytes[3];
    int result;

    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *value = 0;

    result = pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        bytes,
        sizeof(bytes)
    );

    if (result < 0) {
        return result;
    }

    *value =
        ((u32)bytes[0] << 16) |
        ((u32)bytes[1] << 8) |
        (u32)bytes[2];

    return PACH_MEMORY_OK;
}

int pach_memory_read_u32_be(
    const PachMemoryMap *map,
    u32 source_address,
    PachAddressKind address_kind,
    u32 *value)
{
    u8 bytes[4];
    int result;

    if (value == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    *value = 0;

    result = pach_memory_read_bytes(
        map,
        source_address,
        address_kind,
        bytes,
        sizeof(bytes)
    );

    if (result < 0) {
        return result;
    }

    *value =
        ((u32)bytes[0] << 24) |
        ((u32)bytes[1] << 16) |
        ((u32)bytes[2] << 8) |
        (u32)bytes[3];

    return PACH_MEMORY_OK;
}

int pach_memory_self_test(const PachMemoryMap *map)
{
    u32 translated_address;
    u32 rejected_value;

    int result;

    if (map == NULL) {
        return PACH_MEMORY_ERROR_ARGUMENT;
    }

    /*
     * Начало RA memory map соответствует
     * kernel RAM PSP.
     */
    result = pach_memory_translate(
        map,
        0x00000000u,
        PACH_ADDRESS_OFFSET,
        1,
        &translated_address
    );

    if (result < 0 ||
        translated_address != 0x88000000u) {

        return PACH_MEMORY_ERROR_TEST;
    }

    /*
     * Начало пользовательской памяти:
     *
     * RA:  0x00800000
     * PSP: 0x08800000
     * K1:  0x88800000
     */
    result = pach_memory_translate(
        map,
        0x00800000u,
        PACH_ADDRESS_OFFSET,
        1,
        &translated_address
    );

    if (result < 0 ||
        translated_address != 0x88800000u) {

        return PACH_MEMORY_ERROR_TEST;
    }

    result = pach_memory_translate(
        map,
        0x08804000u,
        PACH_ADDRESS_USER,
        4,
        &translated_address
    );

    if (result < 0 ||
        translated_address != 0x88804000u) {

        return PACH_MEMORY_ERROR_TEST;
    }

    result = pach_memory_translate(
        map,
        0x88804000u,
        PACH_ADDRESS_KERNEL,
        4,
        &translated_address
    );

    if (result < 0 ||
        translated_address != 0x88804000u) {

        return PACH_MEMORY_ERROR_TEST;
    }

    result = pach_memory_translate(
        map,
        0x01FFFFFFu,
        PACH_ADDRESS_OFFSET,
        1,
        &translated_address
    );

    if (result < 0 ||
        translated_address != 0x89FFFFFFu) {

        return PACH_MEMORY_ERROR_TEST;
    }

    result = pach_memory_translate(
        map,
        0x01FFFFFFu,
        PACH_ADDRESS_OFFSET,
        2,
        &translated_address
    );

    if (result != PACH_MEMORY_ERROR_RANGE) {
        return PACH_MEMORY_ERROR_TEST;
    }

    result = pach_memory_translate(
        map,
        0x02000000u,
        PACH_ADDRESS_OFFSET,
        1,
        &translated_address
    );

    if (result != PACH_MEMORY_ERROR_RANGE) {
        return PACH_MEMORY_ERROR_TEST;
    }

    /*
     * Проверяем, что read32 тоже отклоняет
     * чтение через границу памяти.
     *
     * Фактического обращения к памяти здесь
     * произойти не должно.
     */
    result = pach_memory_read_u32_le(
        map,
        0x01FFFFFEu,
        PACH_ADDRESS_OFFSET,
        &rejected_value
    );

    if (result != PACH_MEMORY_ERROR_RANGE) {
        return PACH_MEMORY_ERROR_TEST;
    }

    return PACH_MEMORY_OK;
}