#ifndef FORMAT_H
#define FORMAT_H
#define PACH_VERSION            2
#define PACH_GAME_MAGIC_0       'P'
#define PACH_GAME_MAGIC_1       'A'
#define PACH_GAME_MAGIC_2       'C'
#define PACH_GAME_MAGIC_3       'H'
#define PACH_PROFILE_MAGIC_0    'P'
#define PACH_PROFILE_MAGIC_1    'P'
#define PACH_PROFILE_MAGIC_2    'R'
#define PACH_PROFILE_MAGIC_3    'F'
#define PACH_MAX_TITLE_LEN      48
#define PACH_MAX_DESC_LEN       96
#define PACH_MAX_GAME_CODE_LEN  16
#define PACH_MAX_PROFILE_NAME   32
#define PACH_MAX_PROFILE_GAMES  16
#define PACH_MAX_GAME_ACH       80
#define PACH_MAX_RA_LOGIC_LEN   852

typedef struct {
    char magic[4];
    int version;
    int game_id;
    char game_code[PACH_MAX_GAME_CODE_LEN];
    int num_achievements;
} PACH_GameFileHeader;

typedef struct {
    int id;
    char title[PACH_MAX_TITLE_LEN];
    char desc[PACH_MAX_DESC_LEN];
    int points;
    char ra_logic[PACH_MAX_RA_LOGIC_LEN];
} PACH_AchievementDef;

typedef struct {
    char magic[4];
    int version;
    char username[PACH_MAX_PROFILE_NAME];
    int num_games;
} PACH_ProfileHeader;

typedef struct {
    int game_id;
    int num_achievements;
    unsigned char unlocked[PACH_MAX_GAME_ACH];
} PACH_ProfileGameProgress;

typedef struct {
    char game_code[PACH_MAX_GAME_CODE_LEN];
    int game_id;
    char ach_file[32];
} PACH_GameMapEntry;

#endif