#ifndef PACH_MEMORY_H
#define PACH_MEMORY_H

unsigned char  pach_mem_read8(unsigned int ra_addr);
unsigned short pach_mem_read16(unsigned int ra_addr);
unsigned int   pach_mem_read32(unsigned int ra_addr);
unsigned char  pach_mem_read_bit0(unsigned int ra_addr);
float          pach_mem_read_float_be(unsigned int ra_addr);
int            pach_mem_valid(unsigned int ra_addr);

#endif