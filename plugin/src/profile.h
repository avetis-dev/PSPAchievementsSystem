#ifndef PROFILE_H
#define PROFILE_H

#include "format.h"

typedef struct {
    PACH_ProfileHeader header;
    PACH_ProfileGameProgress games[PACH_MAX_PROFILE_GAMES];
} PACH_ProfileData;

int  pach_profile_ensure_dirs(void);
int  pach_profile_get_active_name(char *out_name, int max_len);
int  pach_profile_set_active_name(const char *name);
int  pach_profile_load(PACH_ProfileData *profile, const char *name);
int  pach_profile_save(PACH_ProfileData *profile);
void pach_profile_init_empty(PACH_ProfileData *profile, const char *name);

PACH_ProfileGameProgress *pach_profile_find_game(PACH_ProfileData *profile, int game_id);
PACH_ProfileGameProgress *pach_profile_get_or_create_game(PACH_ProfileData *profile,
                                                          int game_id,
                                                          int num_achievements);

int  pach_profile_is_unlocked(PACH_ProfileGameProgress *gp, int ach_index);
void pach_profile_set_unlocked(PACH_ProfileGameProgress *gp, int ach_index);

#endif /* PROFILE_H */