#ifndef PACH_SOUND_H
#define PACH_SOUND_H

#include <psptypes.h>

typedef enum PachSoundKind {
    PACH_SOUND_KIND_STATUS = 0,
    PACH_SOUND_KIND_ACHIEVEMENT = 1
} PachSoundKind;

typedef struct PachSound {
    int channel;
    int volume;
    int initialized;
} PachSound;

enum PachSoundResult {
    PACH_SOUND_OK = 0,
    PACH_SOUND_BUSY = 1,

    PACH_SOUND_ERROR_ARGUMENT = -10001,
    PACH_SOUND_ERROR_RESERVE  = -10002,
    PACH_SOUND_ERROR_OUTPUT   = -10003
};

void pach_sound_reset(
    PachSound *sound
);

int pach_sound_init(
    PachSound *sound,
    u32 volume_percent
);

void pach_sound_tick(
    PachSound *sound
);

int pach_sound_play(
    PachSound *sound,
    PachSoundKind kind
);

void pach_sound_shutdown(
    PachSound *sound
);

#endif
