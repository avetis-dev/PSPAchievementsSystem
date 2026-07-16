#ifndef PACH_LOGGER_H
#define PACH_LOGGER_H

#include <psptypes.h>

int pach_log_init(void);

int pach_log_line(
    const char *message
);

int pach_log_key_value(
    const char *key,
    const char *value
);

int pach_log_hex32(
    const char *key,
    u32 value
);

#endif