#ifndef PACH_ACHIEVEMENT_H
#define PACH_ACHIEVEMENT_H

#include <psptypes.h>

#include "pach_memory.h"

#define PACH_ACHIEVEMENT_MAX             128u
#define PACH_ACHIEVEMENT_TITLE_CAPACITY   64u
#define PACH_ACHIEVEMENT_GROUP_MAX       256u
#define PACH_ACHIEVEMENT_CONDITION_MAX  3072u
#define PACH_ACHIEVEMENT_OPERAND_MAX    (PACH_ACHIEVEMENT_CONDITION_MAX * 2u)
#define PACH_ACHIEVEMENT_STRING_MAX    32768u
#define PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX 256u
#define PACH_ACHIEVEMENT_SCHEDULER_WATCH_MAX 8u
#define PACH_ACHIEVEMENT_SCHEDULER_CYCLE 48u

#define PACH_OPERAND_MEMORY_INDEX_NONE 0xFFFFu

typedef enum PachOperandType {
    PACH_OPERAND_VALUE_U32 = 0,
    PACH_OPERAND_VALUE_FLOAT = 1,
    PACH_OPERAND_MEMORY_U8 = 2,
    PACH_OPERAND_MEMORY_U32 = 3,
    PACH_OPERAND_MEMORY_FLOAT = 4,
    PACH_OPERAND_MEMORY_BIT0 = 5,
    PACH_OPERAND_MEMORY_U16 = 6,
    PACH_OPERAND_MEMORY_U24 = 7,
    PACH_OPERAND_MEMORY_U16_BE = 8,
    PACH_OPERAND_MEMORY_U24_BE = 9,
    PACH_OPERAND_MEMORY_U32_BE = 10,
    PACH_OPERAND_MEMORY_BIT1 = 11,
    PACH_OPERAND_MEMORY_BIT2 = 12,
    PACH_OPERAND_MEMORY_BIT3 = 13,
    PACH_OPERAND_MEMORY_BIT4 = 14,
    PACH_OPERAND_MEMORY_BIT5 = 15,
    PACH_OPERAND_MEMORY_BIT6 = 16,
    PACH_OPERAND_MEMORY_BIT7 = 17,
    PACH_OPERAND_MEMORY_LOWER4 = 18,
    PACH_OPERAND_MEMORY_UPPER4 = 19,
    PACH_OPERAND_MEMORY_BITCOUNT = 20
} PachOperandType;

typedef enum PachOperandState {
    PACH_OPERAND_STATE_CURRENT = 0,
    PACH_OPERAND_STATE_DELTA = 1,
    PACH_OPERAND_STATE_PRIOR = 2
} PachOperandState;

typedef enum PachComparison {
    PACH_COMPARE_EQUAL = 0,
    PACH_COMPARE_NOT_EQUAL = 1,
    PACH_COMPARE_LESS = 2,
    PACH_COMPARE_LESS_EQUAL = 3,
    PACH_COMPARE_GREATER = 4,
    PACH_COMPARE_GREATER_EQUAL = 5,
    PACH_COMPARE_NONE = 0xFF
} PachComparison;

typedef enum PachConditionFlag {
    PACH_CONDITION_STANDARD = 0,
    PACH_CONDITION_PAUSE_IF = 1,
    PACH_CONDITION_RESET_IF = 2,
    PACH_CONDITION_RESET_NEXT_IF = 3,
    PACH_CONDITION_ADD_SOURCE = 4,
    PACH_CONDITION_ADD_ADDRESS = 5,
    PACH_CONDITION_AND_NEXT = 6,
    PACH_CONDITION_OR_NEXT = 7,
    PACH_CONDITION_MEASURED = 8,
    PACH_CONDITION_MEASURED_IF = 9,
    PACH_CONDITION_TRIGGER = 10,
    PACH_CONDITION_SUB_SOURCE = 11,
    PACH_CONDITION_ADD_HITS = 12,
    PACH_CONDITION_SUB_HITS = 13
} PachConditionFlag;

typedef struct PachOperand {
    u8 type;
    u8 state;
    u16 memory_index;
    u32 value;
} PachOperand;

typedef struct PachAchievementCondition {
    PachOperand left;
    PachOperand right;
    u8 comparison;
    u8 flags;
    u16 hit_target;
} PachAchievementCondition;

typedef struct PachAchievementGroup {
    u32 first_condition;
    u16 condition_count;
    u16 flags;
} PachAchievementGroup;

typedef struct PachAchievementDefinition {
    u32 id;
    u16 points;
    u8 type;
    u8 flags;
    u16 first_group;
    u16 group_count;
    u32 title_offset;
    u32 description_offset;
    u32 badge_id;
    u32 author_offset;
    u32 reserved;
} PachAchievementDefinition;

typedef struct PachAchievementRuntime {
    u8 unlocked;
    u8 waiting;
    u16 reserved;
} PachAchievementRuntime;

typedef struct PachConditionRuntime {
    u16 current_hits;
    u16 reserved;
} PachConditionRuntime;

typedef struct PachOperandRuntime {
    u32 effective_address;
    u32 current_value;
    u32 previous_value;
    u32 prior_value;
    u8 initialized;
    u8 reserved[3];
} PachOperandRuntime;

typedef struct PachSampleCacheEntry {
    u32 effective_address;
    u32 value;
    u32 generation;
    u8 operand_type;
    u8 reserved[3];
} PachSampleCacheEntry;

typedef struct PachSchedulerWatch {
    u32 effective_address;
    u32 value;
    u16 occurrences;
    u8 operand_type;
    u8 initialized;
} PachSchedulerWatch;

typedef struct PachAchievementEngine {
    PachAchievementDefinition *definitions;
    u32 achievement_count;

    PachAchievementGroup *groups;
    u32 group_count;

    PachAchievementCondition *conditions;
    u32 condition_count;

    PachAchievementRuntime runtime[PACH_ACHIEVEMENT_MAX];
    PachConditionRuntime condition_runtime[PACH_ACHIEVEMENT_CONDITION_MAX];
    PachOperandRuntime operand_runtime[PACH_ACHIEVEMENT_OPERAND_MAX];
    u8 group_paused[PACH_ACHIEVEMENT_GROUP_MAX];
    PachSampleCacheEntry sample_cache[PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX];
    PachSchedulerWatch scheduler_watches[PACH_ACHIEVEMENT_SCHEDULER_WATCH_MAX];
    u8 schedule_period[PACH_ACHIEVEMENT_MAX];
    u8 schedule_phase[PACH_ACHIEVEMENT_MAX];

    u32 memory_ref_count;
    u32 sample_generation;
    u32 sample_cache_hits;
    u32 sample_cache_misses;
    u32 scheduler_tick;
    u32 scheduler_burst_ticks;
    u32 scheduler_full_scans;
    u32 scheduler_partial_scans;
    u32 scheduler_peak_conditions;
    u8 scheduler_enabled;
    u8 scheduler_watch_count;
    u16 scheduler_reserved;
    int initialized;
} PachAchievementEngine;


typedef enum PachAchievementSchedulerMode {
    PACH_ACHIEVEMENT_SCHEDULER_AUTO = 0,
    PACH_ACHIEVEMENT_SCHEDULER_FULL = 1,
    PACH_ACHIEVEMENT_SCHEDULER_ADAPTIVE = 2
} PachAchievementSchedulerMode;

typedef struct PachAchievementProgress {
    u32 current;
    u32 target;
    u8 available;
    u8 measured;
    u16 reserved;
} PachAchievementProgress;

typedef struct PachAchievementEvent {
    u32 achievement_index;
    u32 achievement_id;
} PachAchievementEvent;

enum PachAchievementResult {
    PACH_ACHIEVEMENT_TICK_IDLE = 0,
    PACH_ACHIEVEMENT_TICK_UNLOCKED = 1,

    PACH_ACHIEVEMENT_ERROR_ARGUMENT = -6001,
    PACH_ACHIEVEMENT_ERROR_COUNT = -6002,
    PACH_ACHIEVEMENT_ERROR_OPERAND = -6003,
    PACH_ACHIEVEMENT_ERROR_COMPARISON = -6004,
    PACH_ACHIEVEMENT_ERROR_EVENTS = -6005,
    PACH_ACHIEVEMENT_ERROR_GROUP = -6006,
    PACH_ACHIEVEMENT_ERROR_CONDITION = -6007,
    PACH_ACHIEVEMENT_ERROR_MEMREF = -6008,
    PACH_ACHIEVEMENT_ERROR_ADDRESS = -6009,
    PACH_ACHIEVEMENT_ERROR_CHAIN = -6010
};

void pach_achievement_engine_reset(
    PachAchievementEngine *engine
);

int pach_achievement_engine_init(
    PachAchievementEngine *engine,
    PachAchievementDefinition *definitions,
    u32 achievement_count,
    PachAchievementGroup *groups,
    u32 group_count,
    PachAchievementCondition *conditions,
    u32 condition_count
);


int pach_achievement_engine_configure_scheduler(
    PachAchievementEngine *engine,
    u8 mode
);

int pach_achievement_engine_get_progress(
    const PachAchievementEngine *engine,
    u32 achievement_index,
    PachAchievementProgress *progress
);

int pach_achievement_engine_restore_ids(
    PachAchievementEngine *engine,
    const u32 *achievement_ids,
    u32 achievement_id_count,
    u32 *restored_count
);

int pach_achievement_engine_tick(
    PachAchievementEngine *engine,
    const PachMemoryMap *memory_map,
    PachAchievementEvent *events,
    u32 event_capacity,
    u32 *event_count
);

#endif
