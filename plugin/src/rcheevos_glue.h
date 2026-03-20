#ifndef RCHEEVOS_GLUE_H
#define RCHEEVOS_GLUE_H

#include "format.h"
#include "game_db.h"
#include "profile.h"

/* Снижаем лимиты для экономии RAM */
#define RC_MAX_CONDITIONS    24
#define RC_MAX_GROUPS        4
#define RC_MAX_DELTA_SLOTS   128

typedef enum { RC_MEMSIZE_8BIT, RC_MEMSIZE_16BIT, RC_MEMSIZE_32BIT, RC_MEMSIZE_BIT0, RC_MEMSIZE_FLOAT_BE, RC_MEMSIZE_CONST_UINT, RC_MEMSIZE_CONST_FLOAT } RC_MemSize;
typedef struct { RC_MemSize size; unsigned int address; unsigned int const_uint; float const_float; int is_delta; int is_prior; } RC_Operand;
typedef enum { RC_OP_EQ, RC_OP_NE, RC_OP_LT, RC_OP_LE, RC_OP_GT, RC_OP_GE, RC_OP_NONE } RC_CompOp;
typedef enum { RC_COND_STANDARD = 0, RC_COND_RESET_IF, RC_COND_PAUSE_IF, RC_COND_AND_NEXT, RC_COND_OR_NEXT, RC_COND_TRIGGER, RC_COND_MEASURED, RC_COND_MEASURED_IF, RC_COND_ADD_SOURCE, RC_COND_ADD_ADDRESS, RC_COND_RESET_NEXT_IF, RC_COND_SUB_SOURCE } RC_CondType;

typedef struct { RC_CondType type; RC_Operand left; RC_CompOp op; RC_Operand right; unsigned int required_hits; unsigned int current_hits; } RC_Condition;
typedef struct { RC_Condition conds[RC_MAX_CONDITIONS]; int count; } RC_CondGroup;
typedef struct { RC_CondGroup groups[RC_MAX_GROUPS]; int num_groups; int is_active; } RC_ParsedAchievement;
typedef struct { unsigned int addr; RC_MemSize size; float current; float delta; float prior; } RC_DeltaSlot;
typedef struct { RC_DeltaSlot slots[RC_MAX_DELTA_SLOTS]; int num_slots; } RC_RuntimeState;
typedef struct { int unlocked_index; PACH_AchievementDef *unlocked_def; } RC_EvalResult;

void rc_glue_init(RC_RuntimeState *state);
int  rc_glue_parse(const char *logic, RC_ParsedAchievement *out);
int  rc_glue_parse_all(PACH_LoadedGame *game, RC_ParsedAchievement *out_array, int max_count);
RC_EvalResult rc_glue_update(PACH_LoadedGame *game, PACH_ProfileGameProgress *progress, RC_RuntimeState *state, RC_ParsedAchievement *parsed_cache, int num_parsed);

#endif