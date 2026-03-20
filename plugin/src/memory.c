#include <string.h>
#include "memory.h"

/* 
 * PSP Memory mapping for RetroAchievements:
 *
 * RA addresses as used by PPSSPP start from 0x08000000
 * (not 0x08800000 as the kernel user space starts).
 *
 * PSP has RAM at:
 *   0x08000000 - 0x09FFFFFF (32MB total on Slim/3000)
 *   0x08800000 - 0x09FFFFFF (24MB on Fat 1000)
 *
 * RA addr 0x009fda18 + BASE 0x08000000 = real 0x089FDA18
 *
 * We validate that the final address is within accessible range.
 */
#define PSP_RAM_BASE    0x08000000u
#define PSP_RAM_END     0x0A000000u

int pach_mem_valid(unsigned int ra_addr) {
    unsigned int real = PSP_RAM_BASE + ra_addr;
    return (real >= PSP_RAM_BASE && real < PSP_RAM_END) ? 1 : 0;
}

static unsigned char safe_read8(unsigned int ra_addr) {
    unsigned int real = PSP_RAM_BASE + ra_addr;
    if (real < 0x08800000 || real >= PSP_RAM_END) return 0;
    return *((volatile unsigned char *)real);
}

unsigned char pach_mem_read8(unsigned int ra_addr) {
    return safe_read8(ra_addr);
}

unsigned short pach_mem_read16(unsigned int ra_addr) {
    unsigned char b0 = safe_read8(ra_addr);
    unsigned char b1 = safe_read8(ra_addr + 1);
    return (unsigned short)b0 | ((unsigned short)b1 << 8);
}

unsigned int pach_mem_read32(unsigned int ra_addr) {
    unsigned char b0 = safe_read8(ra_addr);
    unsigned char b1 = safe_read8(ra_addr + 1);
    unsigned char b2 = safe_read8(ra_addr + 2);
    unsigned char b3 = safe_read8(ra_addr + 3);
    return (unsigned int)b0 |
           ((unsigned int)b1 << 8) |
           ((unsigned int)b2 << 16) |
           ((unsigned int)b3 << 24);
}

unsigned char pach_mem_read_bit0(unsigned int ra_addr) {
    return safe_read8(ra_addr) & 0x01;
}

float pach_mem_read_float_be(unsigned int ra_addr) {
    unsigned int raw = pach_mem_read32(ra_addr);
    float f;
    memcpy(&f, &raw, 4);
    return f;
}