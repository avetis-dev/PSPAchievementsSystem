#include "pach_achievement.h"

#include <stddef.h>

typedef union PachFloatBits {
    u32 bits;
    float value;
} PachFloatBits;

typedef struct PachTypedValue {
    u32 bits;
    u8 is_float;
    u8 valid;
    u16 reserved;
} PachTypedValue;

typedef struct PachConditionSegment {
    u32 start;
    u32 end;
    u8 category;
    u8 reserved[3];
} PachConditionSegment;

typedef enum PachEvaluationPass {
    PACH_PASS_PAUSE = 0,
    PACH_PASS_RESET,
    PACH_PASS_RESET_NEXT,
    PACH_PASS_NORMAL
} PachEvaluationPass;

#define PACH_SCHEDULER_ENABLE_CONDITION_THRESHOLD 1024u
#define PACH_SCHEDULER_WATCH_CANDIDATE_MAX 256u
#define PACH_SCHEDULER_BURST_TICKS 3u
#define PACH_SCHEDULER_EXTREME_CONDITION_THRESHOLD 8192u

typedef struct PachSchedulerWatchCandidate {
    u32 effective_address;
    u16 occurrences;
    u8 operand_type;
    u8 used;
    u8 selected;
    u8 reserved[3];
} PachSchedulerWatchCandidate;

static int pach_operand_is_memory(u8 type)
{
    return
        type >= PACH_OPERAND_MEMORY_U8 &&
        type <= PACH_OPERAND_MEMORY_BITCOUNT;
}

static int pach_operand_is_float(u8 type)
{
    return
        type == PACH_OPERAND_VALUE_FLOAT ||
        type == PACH_OPERAND_MEMORY_FLOAT;
}

static int pach_condition_is_modifier(u8 flags)
{
    return
        flags == PACH_CONDITION_ADD_SOURCE ||
        flags == PACH_CONDITION_SUB_SOURCE ||
        flags == PACH_CONDITION_ADD_ADDRESS;
}

static int pach_condition_is_hit_modifier(u8 flags)
{
    return
        flags == PACH_CONDITION_ADD_HITS ||
        flags == PACH_CONDITION_SUB_HITS;
}

static int pach_condition_is_link(u8 flags)
{
    return
        flags == PACH_CONDITION_AND_NEXT ||
        flags == PACH_CONDITION_OR_NEXT;
}

static int pach_operand_is_valid(const PachOperand *operand)
{
    if (operand == NULL) {
        return 0;
    }

    if (operand->type > PACH_OPERAND_MEMORY_BITCOUNT ||
        operand->state > PACH_OPERAND_STATE_PRIOR) {

        return 0;
    }

    if (!pach_operand_is_memory(operand->type) &&
        operand->state != PACH_OPERAND_STATE_CURRENT) {

        return 0;
    }

    if (!pach_operand_is_memory(operand->type) &&
        operand->memory_index != PACH_OPERAND_MEMORY_INDEX_NONE) {

        return 0;
    }

    if (pach_operand_is_memory(operand->type) &&
        operand->memory_index != PACH_OPERAND_MEMORY_INDEX_NONE) {

        if (operand->memory_index < PACH_OPERAND_DIVISOR_MIN ||
            operand->memory_index > PACH_OPERAND_DIVISOR_MAX ||
            pach_operand_is_float(operand->type)) {

            return 0;
        }
    }

    return 1;
}

static u32 pach_operand_runtime_index(
    u32 condition_index,
    int right_operand)
{
    return condition_index * 2u +
        (right_operand ? 1u : 0u);
}

static int pach_normalize_effective_address(
    u32 effective_address,
    u32 *ra_address)
{
    if (ra_address == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (effective_address < PACH_MEMORY_SIZE_32_MB) {
        *ra_address = effective_address;
        return PACH_MEMORY_OK;
    }

    if (effective_address >= 0x08000000u &&
        effective_address < 0x0A000000u) {

        *ra_address = effective_address - 0x08000000u;
        return PACH_MEMORY_OK;
    }

    if (effective_address >= 0x88000000u &&
        effective_address < 0x8A000000u) {

        *ra_address = effective_address - 0x88000000u;
        return PACH_MEMORY_OK;
    }

    return PACH_ACHIEVEMENT_ERROR_ADDRESS;
}

static u32 pach_count_bits_u8(u8 value)
{
    u32 count = 0;

    while (value != 0) {
        count += (u32)(value & 1u);
        value >>= 1;
    }

    return count;
}

static int pach_read_memory_bits(
    const PachMemoryMap *memory_map,
    u8 operand_type,
    u32 effective_address,
    u32 *value)
{
    u32 ra_address;
    int result;

    if (memory_map == NULL || value == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    result = pach_normalize_effective_address(
        effective_address,
        &ra_address
    );

    if (result < 0) {
        /*
         * Pointer chains can temporarily resolve outside PSP RAM while a
         * scene is loading or before the base pointer is initialized. RA
         * clients treat unreadable memory as zero instead of disabling the
         * entire achievement runtime.
         */
        *value = 0;
        return PACH_MEMORY_OK;
    }

    switch (operand_type) {
    case PACH_OPERAND_MEMORY_U8: {
        u8 value8 = 0;

        result = pach_memory_read_u8(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            &value8
        );

        *value = (u32)value8;
        return result;
    }

    case PACH_OPERAND_MEMORY_U16: {
        u16 value16 = 0;

        result = pach_memory_read_u16_le(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            &value16
        );

        *value = (u32)value16;
        return result;
    }

    case PACH_OPERAND_MEMORY_U24:
        return pach_memory_read_u24_le(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            value
        );

    case PACH_OPERAND_MEMORY_U32:
    case PACH_OPERAND_MEMORY_FLOAT:
        return pach_memory_read_u32_le(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            value
        );

    case PACH_OPERAND_MEMORY_U16_BE: {
        u16 value16 = 0;

        result = pach_memory_read_u16_be(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            &value16
        );

        *value = (u32)value16;
        return result;
    }

    case PACH_OPERAND_MEMORY_U24_BE:
        return pach_memory_read_u24_be(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            value
        );

    case PACH_OPERAND_MEMORY_U32_BE:
        return pach_memory_read_u32_be(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            value
        );

    case PACH_OPERAND_MEMORY_BIT0:
    case PACH_OPERAND_MEMORY_BIT1:
    case PACH_OPERAND_MEMORY_BIT2:
    case PACH_OPERAND_MEMORY_BIT3:
    case PACH_OPERAND_MEMORY_BIT4:
    case PACH_OPERAND_MEMORY_BIT5:
    case PACH_OPERAND_MEMORY_BIT6:
    case PACH_OPERAND_MEMORY_BIT7: {
        u8 value8 = 0;
        u32 bit_index =
            operand_type == PACH_OPERAND_MEMORY_BIT0
                ? 0u
                : 1u +
                    ((u32)operand_type -
                     (u32)PACH_OPERAND_MEMORY_BIT1);

        result = pach_memory_read_u8(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            &value8
        );

        *value = (u32)((value8 >> bit_index) & 1u);
        return result;
    }

    case PACH_OPERAND_MEMORY_LOWER4:
    case PACH_OPERAND_MEMORY_UPPER4:
    case PACH_OPERAND_MEMORY_BITCOUNT: {
        u8 value8 = 0;

        result = pach_memory_read_u8(
            memory_map,
            ra_address,
            PACH_ADDRESS_OFFSET,
            &value8
        );

        if (result < 0) {
            *value = 0;
            return result;
        }

        if (operand_type == PACH_OPERAND_MEMORY_LOWER4) {
            *value = (u32)(value8 & 0x0Fu);
        } else if (operand_type == PACH_OPERAND_MEMORY_UPPER4) {
            *value = (u32)((value8 >> 4) & 0x0Fu);
        } else {
            *value = pach_count_bits_u8(value8);
        }

        return PACH_MEMORY_OK;
    }

    default:
        return PACH_ACHIEVEMENT_ERROR_OPERAND;
    }
}

static void pach_sample_cache_begin(
    PachAchievementEngine *engine)
{
    u32 index;

    if (engine == NULL) {
        return;
    }

    ++engine->sample_generation;

    if (engine->sample_generation == 0) {
        for (index = 0;
             index < PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX;
             ++index) {

            engine->sample_cache[index].generation = 0;
        }

        engine->sample_generation = 1;
    }

    engine->sample_cache_hits = 0;
    engine->sample_cache_misses = 0;
}

static u32 pach_sample_cache_hash(
    u8 operand_type,
    u32 effective_address)
{
    u32 hash = effective_address;

    hash ^= effective_address >> 7;
    hash ^= effective_address >> 15;
    hash ^= (u32)operand_type * 0x9E3779B1u;

    return hash &
        (PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX - 1u);
}

static int pach_read_memory_cached(
    PachAchievementEngine *engine,
    const PachMemoryMap *memory_map,
    u8 operand_type,
    u32 effective_address,
    u32 *value)
{
    u32 start;
    u32 probe;

    if (engine == NULL ||
        memory_map == NULL ||
        value == NULL) {

        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    start = pach_sample_cache_hash(
        operand_type,
        effective_address
    );

    for (probe = 0;
         probe < PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX;
         ++probe) {

        u32 index =
            (start + probe) &
            (PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX - 1u);

        PachSampleCacheEntry *entry =
            &engine->sample_cache[index];

        if (entry->generation !=
            engine->sample_generation) {

            int result = pach_read_memory_bits(
                memory_map,
                operand_type,
                effective_address,
                value
            );

            if (result < 0) {
                return result;
            }

            entry->effective_address = effective_address;
            entry->value = *value;
            entry->generation = engine->sample_generation;
            entry->operand_type = operand_type;
            entry->reserved[0] = 0;
            entry->reserved[1] = 0;
            entry->reserved[2] = 0;

            ++engine->sample_cache_misses;
            return PACH_MEMORY_OK;
        }

        if (entry->effective_address == effective_address &&
            entry->operand_type == operand_type) {

            *value = entry->value;
            ++engine->sample_cache_hits;
            return PACH_MEMORY_OK;
        }
    }

    /*
     * Correctness is more important than caching. If a package touches more
     * unique locations than the fixed cache can hold, read the remaining
     * operands directly instead of failing the achievement engine.
     */
    ++engine->sample_cache_misses;

    return pach_read_memory_bits(
        memory_map,
        operand_type,
        effective_address,
        value
    );
}

static u32 pach_definition_condition_count(
    const PachAchievementEngine *engine,
    const PachAchievementDefinition *definition)
{
    u32 group_offset;
    u32 count = 0;

    if (engine == NULL || definition == NULL) {
        return 0;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index =
            (u32)definition->first_group +
            group_offset;

        if (group_index >= engine->group_count) {
            return 0;
        }

        count += engine->groups[group_index].condition_count;
    }

    return count;
}

static int pach_definition_has_flag(
    const PachAchievementEngine *engine,
    const PachAchievementDefinition *definition,
    u8 first_flag,
    u8 last_flag)
{
    u32 group_offset;

    if (engine == NULL || definition == NULL) {
        return 0;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index =
            (u32)definition->first_group +
            group_offset;

        const PachAchievementGroup *group;
        u32 local_index;

        if (group_index >= engine->group_count) {
            return 0;
        }

        group = &engine->groups[group_index];

        for (local_index = 0;
             local_index < group->condition_count;
             ++local_index) {

            const PachAchievementCondition *condition =
                &engine->conditions[
                    group->first_condition + local_index
                ];

            if (condition->flags >= first_flag &&
                condition->flags <= last_flag) {

                return 1;
            }
        }
    }

    return 0;
}

static int pach_definition_is_hit_sensitive(
    const PachAchievementEngine *engine,
    const PachAchievementDefinition *definition)
{
    u32 group_offset;

    if (engine == NULL || definition == NULL) {
        return 0;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index =
            (u32)definition->first_group +
            group_offset;

        const PachAchievementGroup *group;
        u32 local_index;
        int hit_chain_active = 0;

        if (group_index >= engine->group_count) {
            return 0;
        }

        group = &engine->groups[group_index];

        for (local_index = 0;
             local_index < group->condition_count;
             ++local_index) {

            const PachAchievementCondition *condition =
                &engine->conditions[
                    group->first_condition + local_index
                ];

            /*
             * AddHits/SubHits chains commonly aggregate persistent flags.
             * A one-hit cap makes each source latch once, so the chain does
             * not require a full evaluation every video frame. The overall
             * hit target on the final condition is an aggregate threshold,
             * not a frame timer. This distinction is critical for large
             * inventory/collection achievements such as Peace Walker's.
             */
            if (pach_condition_is_hit_modifier(condition->flags)) {
                if (condition->hit_target != 1u) {
                    return 1;
                }

                hit_chain_active = 1;
                continue;
            }

            if (pach_condition_is_modifier(condition->flags) ||
                pach_condition_is_link(condition->flags)) {

                continue;
            }

            if (condition->hit_target > 1u &&
                !hit_chain_active) {

                return 1;
            }

            hit_chain_active = 0;
        }

        if (hit_chain_active) {
            return 1;
        }
    }

    return 0;
}

static int pach_definition_is_transition_sensitive(
    const PachAchievementEngine *engine,
    const PachAchievementDefinition *definition)
{
    u32 group_offset;

    if (engine == NULL || definition == NULL) {
        return 0;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index =
            (u32)definition->first_group + group_offset;

        const PachAchievementGroup *group;
        u32 local_index;

        if (group_index >= engine->group_count) {
            return 0;
        }

        group = &engine->groups[group_index];

        for (local_index = 0;
             local_index < group->condition_count;
             ++local_index) {

            const PachAchievementCondition *condition =
                &engine->conditions[
                    group->first_condition + local_index
                ];

            if ((pach_operand_is_memory(condition->left.type) &&
                 condition->left.state !=
                    PACH_OPERAND_STATE_CURRENT) ||
                (pach_operand_is_memory(condition->right.type) &&
                 condition->right.state !=
                    PACH_OPERAND_STATE_CURRENT)) {

                return 1;
            }
        }
    }

    return 0;
}

static u8 pach_scheduler_choose_period(
    const PachAchievementEngine *engine,
    const PachAchievementDefinition *definition,
    u32 condition_count)
{
    int has_control;

    /*
     * Delta and Prior operands describe frame transitions. Evaluating them
     * on a distributed schedule can miss a one-frame state change,
     * especially around mission teardown and pointer replacement. Keep
     * transition-sensitive achievements at full rate.
     */
    if (pach_definition_is_transition_sensitive(engine, definition) ||
        pach_definition_is_hit_sensitive(engine, definition)) {

        return 1u;
    }

    /*
     * Very large definitions are long-lived aggregate checks. Spreading
     * them across the 48-frame cycle avoids periodic multi-thousand-
     * condition spikes while transition watches still request a full scan
     * when common Delta/Prior addresses change.
     */
    if (condition_count > 1024u) {
        return 96u;
    }

    if (condition_count > 512u) {
        return 48u;
    }

    if (condition_count > 128u) {
        return 24u;
    }

    if (definition->type == 1u || definition->type == 3u) {
        return condition_count <= 20u ? 3u : 6u;
    }

    has_control =
        pach_definition_has_flag(
            engine,
            definition,
            PACH_CONDITION_PAUSE_IF,
            PACH_CONDITION_RESET_NEXT_IF
        );

    if (has_control) {
        return condition_count <= 20u ? 4u : 8u;
    }

    if (condition_count <= 10u) {
        return 4u;
    }

    if (condition_count <= 20u) {
        return 8u;
    }

    return 16u;
}

static u32 pach_scheduler_watch_hash(
    u8 operand_type,
    u32 effective_address)
{
    u32 hash = effective_address;

    hash ^= effective_address >> 9;
    hash ^= effective_address >> 17;
    hash ^= (u32)operand_type * 0x45D9F3Bu;

    return hash &
        (PACH_SCHEDULER_WATCH_CANDIDATE_MAX - 1u);
}

static void pach_scheduler_add_watch_candidate(
    PachSchedulerWatchCandidate *candidates,
    const PachOperand *operand)
{
    u32 start;
    u32 probe;

    if (candidates == NULL || operand == NULL) {
        return;
    }

    /*
     * Transition watches are built from Delta/Prior operands. They identify
     * the small set of state variables that usually announce mission loads,
     * completion counters, deaths, and other short-lived changes. Dynamic
     * AddAddress offsets are excluded because they are not direct PSP RAM
     * addresses until their pointer chain is resolved.
     */
    if (!pach_operand_is_memory(operand->type) ||
        operand->state == PACH_OPERAND_STATE_CURRENT ||
        operand->value >= PACH_MEMORY_SIZE_32_MB) {

        return;
    }

    start = pach_scheduler_watch_hash(
        operand->type,
        operand->value
    );

    for (probe = 0;
         probe < PACH_SCHEDULER_WATCH_CANDIDATE_MAX;
         ++probe) {

        u32 index =
            (start + probe) &
            (PACH_SCHEDULER_WATCH_CANDIDATE_MAX - 1u);

        PachSchedulerWatchCandidate *candidate =
            &candidates[index];

        if (!candidate->used) {
            candidate->effective_address = operand->value;
            candidate->occurrences = 1u;
            candidate->operand_type = operand->type;
            candidate->used = 1u;
            candidate->selected = 0u;
            candidate->reserved[0] = 0u;
            candidate->reserved[1] = 0u;
            candidate->reserved[2] = 0u;
            return;
        }

        if (candidate->effective_address == operand->value &&
            candidate->operand_type == operand->type) {

            if (candidate->occurrences < 0xFFFFu) {
                ++candidate->occurrences;
            }

            return;
        }
    }
}

static void pach_scheduler_build_watches(
    PachAchievementEngine *engine)
{
    PachSchedulerWatchCandidate
        candidates[PACH_SCHEDULER_WATCH_CANDIDATE_MAX];

    u32 index;
    u32 slot;

    if (engine == NULL) {
        return;
    }

    for (index = 0;
         index < PACH_SCHEDULER_WATCH_CANDIDATE_MAX;
         ++index) {

        candidates[index].effective_address = 0;
        candidates[index].occurrences = 0;
        candidates[index].operand_type = 0;
        candidates[index].used = 0;
        candidates[index].selected = 0;
        candidates[index].reserved[0] = 0;
        candidates[index].reserved[1] = 0;
        candidates[index].reserved[2] = 0;
    }

    for (index = 0; index < engine->condition_count; ++index) {
        const PachAchievementCondition *condition =
            &engine->conditions[index];

        pach_scheduler_add_watch_candidate(
            candidates,
            &condition->left
        );

        pach_scheduler_add_watch_candidate(
            candidates,
            &condition->right
        );
    }

    engine->scheduler_watch_count = 0;

    for (slot = 0;
         slot < PACH_ACHIEVEMENT_SCHEDULER_WATCH_MAX;
         ++slot) {

        u32 best_index = PACH_SCHEDULER_WATCH_CANDIDATE_MAX;
        u16 best_occurrences = 0;

        for (index = 0;
             index < PACH_SCHEDULER_WATCH_CANDIDATE_MAX;
             ++index) {

            if (!candidates[index].used ||
                candidates[index].selected ||
                candidates[index].occurrences <= best_occurrences) {

                continue;
            }

            best_index = index;
            best_occurrences = candidates[index].occurrences;
        }

        if (best_index >= PACH_SCHEDULER_WATCH_CANDIDATE_MAX) {
            break;
        }

        candidates[best_index].selected = 1u;

        engine->scheduler_watches[slot].effective_address =
            candidates[best_index].effective_address;

        engine->scheduler_watches[slot].value = 0;

        engine->scheduler_watches[slot].occurrences =
            candidates[best_index].occurrences;

        engine->scheduler_watches[slot].operand_type =
            candidates[best_index].operand_type;

        engine->scheduler_watches[slot].initialized = 0;
        ++engine->scheduler_watch_count;
    }
}

static int pach_scheduler_initialize(
    PachAchievementEngine *engine,
    u8 mode)
{
    u32 frame_load[PACH_ACHIEVEMENT_SCHEDULER_CYCLE];
    u16 weights[PACH_ACHIEVEMENT_MAX];
    u8 assigned[PACH_ACHIEVEMENT_MAX];
    u32 index;
    u32 assigned_count;

    if (engine == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (mode == PACH_ACHIEVEMENT_SCHEDULER_FULL) {
        engine->scheduler_enabled = 0;
    } else if (mode == PACH_ACHIEVEMENT_SCHEDULER_ADAPTIVE) {
        engine->scheduler_enabled =
            engine->achievement_count > 0 &&
            engine->condition_count > 0;
    } else {
        engine->scheduler_enabled =
            engine->condition_count >
            PACH_SCHEDULER_ENABLE_CONDITION_THRESHOLD;
    }

    engine->scheduler_tick = 0;
    engine->scheduler_burst_ticks = 0;
    engine->scheduler_full_scans = 0;
    engine->scheduler_partial_scans = 0;
    engine->scheduler_peak_conditions = 0;
    engine->scheduler_watch_count = 0;
    engine->scheduler_reserved = 0;

    for (index = 0;
         index < PACH_ACHIEVEMENT_SCHEDULER_CYCLE;
         ++index) {

        frame_load[index] = 0;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_MAX;
         ++index) {

        engine->schedule_period[index] = 1u;
        engine->schedule_phase[index] = 0u;
        weights[index] = 0;
        assigned[index] = 0;
    }

    if (!engine->scheduler_enabled) {
        return PACH_MEMORY_OK;
    }

    for (index = 0;
         index < engine->achievement_count;
         ++index) {

        u32 condition_count =
            pach_definition_condition_count(
                engine,
                &engine->definitions[index]
            );

        if (condition_count == 0 || condition_count > 0xFFFFu) {
            return PACH_ACHIEVEMENT_ERROR_GROUP;
        }

        weights[index] = (u16)condition_count;

        engine->schedule_period[index] =
            pach_scheduler_choose_period(
                engine,
                &engine->definitions[index],
                condition_count
            );
    }

    /*
     * Place the heaviest achievements first and choose the phase that gives
     * the lowest peak work across the common 48-frame schedule cycle. This
     * avoids replacing constant slowdown with a periodic hitch.
     */
    for (assigned_count = 0;
         assigned_count < engine->achievement_count;
         ++assigned_count) {

        u32 achievement_index = engine->achievement_count;
        u16 largest_weight = 0;
        u8 period;
        u8 best_phase = 0;
        u32 best_peak = 0xFFFFFFFFu;
        u32 best_sum = 0xFFFFFFFFu;
        u8 phase;

        for (index = 0;
             index < engine->achievement_count;
             ++index) {

            if (!assigned[index] &&
                weights[index] >= largest_weight) {

                achievement_index = index;
                largest_weight = weights[index];
            }
        }

        if (achievement_index >= engine->achievement_count) {
            return PACH_ACHIEVEMENT_ERROR_GROUP;
        }

        period = engine->schedule_period[achievement_index];

        for (phase = 0; phase < period; ++phase) {
            u32 candidate_peak = 0;
            u32 candidate_sum = 0;
            u32 frame;

            for (frame = phase;
                 frame < PACH_ACHIEVEMENT_SCHEDULER_CYCLE;
                 frame += period) {

                u32 load =
                    frame_load[frame] +
                    (u32)largest_weight;

                if (load > candidate_peak) {
                    candidate_peak = load;
                }

                candidate_sum += load;
            }

            if (candidate_peak < best_peak ||
                (candidate_peak == best_peak &&
                 candidate_sum < best_sum)) {

                best_peak = candidate_peak;
                best_sum = candidate_sum;
                best_phase = phase;
            }
        }

        engine->schedule_phase[achievement_index] = best_phase;
        assigned[achievement_index] = 1u;

        for (index = best_phase;
             index < PACH_ACHIEVEMENT_SCHEDULER_CYCLE;
             index += period) {

            frame_load[index] += (u32)largest_weight;
        }
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_SCHEDULER_CYCLE;
         ++index) {

        if (frame_load[index] > engine->scheduler_peak_conditions) {
            engine->scheduler_peak_conditions = frame_load[index];
        }
    }

    pach_scheduler_build_watches(engine);
    return PACH_MEMORY_OK;
}

static int pach_scheduler_update_watches(
    PachAchievementEngine *engine,
    const PachMemoryMap *memory_map,
    int *changed)
{
    u32 index;

    if (engine == NULL || memory_map == NULL || changed == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    *changed = 0;

    for (index = 0;
         index < engine->scheduler_watch_count;
         ++index) {

        PachSchedulerWatch *watch =
            &engine->scheduler_watches[index];

        u32 value;
        int result = pach_read_memory_cached(
            engine,
            memory_map,
            watch->operand_type,
            watch->effective_address,
            &value
        );

        if (result < 0) {
            return result;
        }

        if (!watch->initialized) {
            watch->value = value;
            watch->initialized = 1u;
            continue;
        }

        if (watch->value != value) {
            watch->value = value;
            *changed = 1;
        }
    }

    return PACH_MEMORY_OK;
}

static int pach_scheduler_achievement_due(
    const PachAchievementEngine *engine,
    u32 achievement_index,
    int full_scan)
{
    u8 period;

    if (engine == NULL ||
        achievement_index >= engine->achievement_count) {

        return 0;
    }

    if (full_scan || !engine->scheduler_enabled) {
        return 1;
    }

    period = engine->schedule_period[achievement_index];

    if (period == 0) {
        return 1;
    }

    return
        (engine->scheduler_tick % period) ==
        engine->schedule_phase[achievement_index];
}

static int pach_sample_operand(
    PachAchievementEngine *engine,
    const PachMemoryMap *memory_map,
    const PachOperand *operand,
    u32 condition_index,
    int right_operand,
    u32 address_accumulator)
{
    PachOperandRuntime *runtime;
    u32 effective_address;
    u32 new_value;
    u32 runtime_index;
    int result;

    if (engine == NULL || memory_map == NULL || operand == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (!pach_operand_is_memory(operand->type)) {
        return PACH_MEMORY_OK;
    }

    runtime_index = pach_operand_runtime_index(
        condition_index,
        right_operand
    );

    if (runtime_index >= PACH_ACHIEVEMENT_OPERAND_MAX) {
        return PACH_ACHIEVEMENT_ERROR_MEMREF;
    }

    effective_address = operand->value + address_accumulator;

    result = pach_read_memory_cached(
        engine,
        memory_map,
        operand->type,
        effective_address,
        &new_value
    );

    if (result < 0) {
        return result;
    }

    runtime = &engine->operand_runtime[runtime_index];

    if (!runtime->initialized) {
        runtime->effective_address = effective_address;
        runtime->current_value = new_value;
        runtime->previous_value = new_value;
        runtime->prior_value = new_value;
        runtime->initialized = 1;
        return PACH_MEMORY_OK;
    }

    /*
     * An indirect AddAddress chain can resolve to a different PSP address
     * when a mission object is replaced. For Delta/Prior operands, the
     * history belongs to the logical operand, not to one transient pointer
     * target. Preserve that history across address changes so completion
     * transitions are not discarded. Current-only operands do not consume
     * the history, but using the same update path keeps behavior coherent.
     */
    runtime->previous_value = runtime->current_value;

    if (new_value != runtime->current_value) {
        runtime->prior_value = runtime->current_value;
    }

    runtime->effective_address = effective_address;
    runtime->current_value = new_value;
    return PACH_MEMORY_OK;
}

static int pach_read_operand_bits(
    const PachAchievementEngine *engine,
    const PachOperand *operand,
    u32 condition_index,
    int right_operand,
    u32 *value)
{
    const PachOperandRuntime *runtime;
    u32 runtime_index;

    if (engine == NULL || operand == NULL || value == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (!pach_operand_is_memory(operand->type)) {
        *value = operand->value;
        return PACH_MEMORY_OK;
    }

    runtime_index = pach_operand_runtime_index(
        condition_index,
        right_operand
    );

    if (runtime_index >= PACH_ACHIEVEMENT_OPERAND_MAX) {
        return PACH_ACHIEVEMENT_ERROR_MEMREF;
    }

    runtime = &engine->operand_runtime[runtime_index];

    if (!runtime->initialized) {
        return PACH_ACHIEVEMENT_ERROR_MEMREF;
    }

    switch (operand->state) {
    case PACH_OPERAND_STATE_CURRENT:
        *value = runtime->current_value;
        break;

    case PACH_OPERAND_STATE_DELTA:
        *value = runtime->previous_value;
        break;

    case PACH_OPERAND_STATE_PRIOR:
        *value = runtime->prior_value;
        break;

    default:
        return PACH_ACHIEVEMENT_ERROR_OPERAND;
    }

    if (operand->memory_index != PACH_OPERAND_MEMORY_INDEX_NONE) {
        if (operand->memory_index < PACH_OPERAND_DIVISOR_MIN ||
            operand->memory_index > PACH_OPERAND_DIVISOR_MAX ||
            pach_operand_is_float(operand->type)) {

            return PACH_ACHIEVEMENT_ERROR_OPERAND;
        }

        *value /= (u32)operand->memory_index;
    }

    return PACH_MEMORY_OK;
}

static int pach_sample_group(
    PachAchievementEngine *engine,
    const PachAchievementGroup *group,
    const PachMemoryMap *memory_map)
{
    u32 address_accumulator = 0;
    u32 local_index;
    int result;

    if (engine == NULL || group == NULL || memory_map == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    for (local_index = 0;
         local_index < group->condition_count;
         ++local_index) {

        u32 condition_index =
            group->first_condition + local_index;

        const PachAchievementCondition *condition =
            &engine->conditions[condition_index];

        result = pach_sample_operand(
            engine,
            memory_map,
            &condition->left,
            condition_index,
            0,
            address_accumulator
        );

        if (result < 0) {
            return result;
        }

        result = pach_sample_operand(
            engine,
            memory_map,
            &condition->right,
            condition_index,
            1,
            address_accumulator
        );

        if (result < 0) {
            return result;
        }

        if (condition->flags ==
            PACH_CONDITION_ADD_ADDRESS) {

            u32 pointer_value;

            if (pach_operand_is_float(
                    condition->left.type)) {

                return PACH_ACHIEVEMENT_ERROR_OPERAND;
            }

            result = pach_read_operand_bits(
                engine,
                &condition->left,
                condition_index,
                0,
                &pointer_value
            );

            if (result < 0) {
                return result;
            }

            address_accumulator += pointer_value;
        } else {
            address_accumulator = 0;
        }
    }

    return PACH_MEMORY_OK;
}

static int pach_typed_from_operand(
    const PachAchievementEngine *engine,
    const PachOperand *operand,
    u32 condition_index,
    int right_operand,
    PachTypedValue *value)
{
    int result;

    if (value == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    result = pach_read_operand_bits(
        engine,
        operand,
        condition_index,
        right_operand,
        &value->bits
    );

    if (result < 0) {
        return result;
    }

    value->is_float = (u8)pach_operand_is_float(
        operand->type
    );
    value->valid = 1;
    value->reserved = 0;

    return PACH_MEMORY_OK;
}

static int pach_typed_add(
    PachTypedValue *accumulator,
    const PachTypedValue *value)
{
    if (accumulator == NULL || value == NULL || !value->valid) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (!accumulator->valid) {
        *accumulator = *value;
        return PACH_MEMORY_OK;
    }

    if (accumulator->is_float != value->is_float) {
        return PACH_ACHIEVEMENT_ERROR_OPERAND;
    }

    if (accumulator->is_float) {
        PachFloatBits left;
        PachFloatBits right;

        left.bits = accumulator->bits;
        right.bits = value->bits;
        left.value += right.value;
        accumulator->bits = left.bits;
    } else {
        accumulator->bits += value->bits;
    }

    return PACH_MEMORY_OK;
}

static int pach_typed_subtract(
    PachTypedValue *accumulator,
    const PachTypedValue *value)
{
    if (accumulator == NULL || value == NULL || !value->valid) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (!accumulator->valid) {
        accumulator->bits = 0;
        accumulator->is_float = value->is_float;
        accumulator->valid = 1;
        accumulator->reserved = 0;
    }

    if (accumulator->is_float != value->is_float) {
        return PACH_ACHIEVEMENT_ERROR_OPERAND;
    }

    if (accumulator->is_float) {
        PachFloatBits left;
        PachFloatBits right;

        left.bits = accumulator->bits;
        right.bits = value->bits;
        left.value -= right.value;
        accumulator->bits = left.bits;
    } else {
        accumulator->bits -= value->bits;
    }

    return PACH_MEMORY_OK;
}

static int pach_compare_u32(
    u32 left,
    u32 right,
    u8 comparison,
    int *condition_met)
{
    if (condition_met == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    switch (comparison) {
    case PACH_COMPARE_EQUAL:
        *condition_met = left == right;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_NOT_EQUAL:
        *condition_met = left != right;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_LESS:
        *condition_met = left < right;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_LESS_EQUAL:
        *condition_met = left <= right;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_GREATER:
        *condition_met = left > right;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_GREATER_EQUAL:
        *condition_met = left >= right;
        return PACH_MEMORY_OK;
    default:
        return PACH_ACHIEVEMENT_ERROR_COMPARISON;
    }
}

static int pach_compare_float(
    u32 left_bits,
    u32 right_bits,
    u8 comparison,
    int *condition_met)
{
    PachFloatBits left;
    PachFloatBits right;

    if (condition_met == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    left.bits = left_bits;
    right.bits = right_bits;

    switch (comparison) {
    case PACH_COMPARE_EQUAL:
        *condition_met = left.value == right.value;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_NOT_EQUAL:
        *condition_met = left.value != right.value;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_LESS:
        *condition_met = left.value < right.value;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_LESS_EQUAL:
        *condition_met = left.value <= right.value;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_GREATER:
        *condition_met = left.value > right.value;
        return PACH_MEMORY_OK;
    case PACH_COMPARE_GREATER_EQUAL:
        *condition_met = left.value >= right.value;
        return PACH_MEMORY_OK;
    default:
        return PACH_ACHIEVEMENT_ERROR_COMPARISON;
    }
}

static int pach_evaluate_raw_condition(
    const PachAchievementEngine *engine,
    const PachAchievementCondition *condition,
    u32 condition_index,
    const PachTypedValue *source,
    int *condition_met)
{
    PachTypedValue left;
    PachTypedValue right;
    int result;

    if (engine == NULL || condition == NULL || condition_met == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    result = pach_typed_from_operand(
        engine,
        &condition->left,
        condition_index,
        0,
        &left
    );

    if (result < 0) {
        return result;
    }

    if (source != NULL && source->valid) {
        result = pach_typed_add(&left, source);

        if (result < 0) {
            return result;
        }
    }

    result = pach_typed_from_operand(
        engine,
        &condition->right,
        condition_index,
        1,
        &right
    );

    if (result < 0) {
        return result;
    }

    if (left.is_float != right.is_float) {
        return PACH_ACHIEVEMENT_ERROR_OPERAND;
    }

    if (left.is_float) {
        return pach_compare_float(
            left.bits,
            right.bits,
            condition->comparison,
            condition_met
        );
    }

    return pach_compare_u32(
        left.bits,
        right.bits,
        condition->comparison,
        condition_met
    );
}

static int pach_apply_hit_target(
    PachConditionRuntime *runtime,
    u16 target,
    int raw_met,
    s32 hit_adjustment,
    int *condition_met)
{
    s32 total_hits;

    if (runtime == NULL || condition_met == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (target == 0) {
        *condition_met = raw_met;
        return PACH_MEMORY_OK;
    }

    if (raw_met && runtime->current_hits < target) {
        ++runtime->current_hits;
    }

    total_hits =
        (s32)runtime->current_hits +
        hit_adjustment;

    *condition_met = total_hits >= (s32)target;
    return PACH_MEMORY_OK;
}

static int pach_apply_hit_modifier(
    PachConditionRuntime *runtime,
    u16 target,
    int raw_met,
    u16 *stored_hits)
{
    u16 limit;

    if (runtime == NULL || stored_hits == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    limit = target != 0 ? target : 0xFFFFu;

    if (raw_met && runtime->current_hits < limit) {
        ++runtime->current_hits;
    }

    *stored_hits = runtime->current_hits;
    return PACH_MEMORY_OK;
}

static int pach_find_segment(
    const PachAchievementEngine *engine,
    const PachAchievementGroup *group,
    u32 cursor,
    PachConditionSegment *segment)
{
    u32 group_end;
    u32 index;
    u8 flags;

    if (engine == NULL || group == NULL || segment == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    group_end = group->first_condition + group->condition_count;

    if (cursor < group->first_condition || cursor >= group_end) {
        return PACH_ACHIEVEMENT_ERROR_CHAIN;
    }

    segment->start = cursor;
    index = cursor;

    while (index < group_end &&
           pach_condition_is_modifier(
               engine->conditions[index].flags)) {

        ++index;
    }

    if (index >= group_end) {
        return PACH_ACHIEVEMENT_ERROR_CHAIN;
    }

    flags = engine->conditions[index].flags;

    while (pach_condition_is_link(flags)) {
        ++index;

        while (index < group_end &&
               pach_condition_is_modifier(
                   engine->conditions[index].flags)) {

            ++index;
        }

        if (index >= group_end) {
            return PACH_ACHIEVEMENT_ERROR_CHAIN;
        }

        flags = engine->conditions[index].flags;
    }

    segment->end = index + 1u;
    segment->category = flags;
    segment->reserved[0] = 0;
    segment->reserved[1] = 0;
    segment->reserved[2] = 0;

    return PACH_MEMORY_OK;
}

static int pach_evaluate_segment(
    PachAchievementEngine *engine,
    const PachConditionSegment *segment,
    s32 hit_adjustment,
    int hit_modifier_mode,
    int *segment_met,
    u16 *stored_hits)
{
    PachTypedValue source;
    u8 previous_link = PACH_CONDITION_STANDARD;
    u32 index;
    int have_result = 0;
    int combined_result = 0;
    int result;

    if (engine == NULL || segment == NULL || segment_met == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    if (hit_modifier_mode && stored_hits == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    source.bits = 0;
    source.is_float = 0;
    source.valid = 0;
    source.reserved = 0;

    if (stored_hits != NULL) {
        *stored_hits = 0;
    }

    for (index = segment->start; index < segment->end; ++index) {
        const PachAchievementCondition *condition =
            &engine->conditions[index];

        if (condition->flags == PACH_CONDITION_ADD_ADDRESS) {
            continue;
        }

        if (condition->flags == PACH_CONDITION_ADD_SOURCE ||
            condition->flags == PACH_CONDITION_SUB_SOURCE) {

            PachTypedValue value;

            result = pach_typed_from_operand(
                engine,
                &condition->left,
                index,
                0,
                &value
            );

            if (result < 0) {
                return result;
            }

            if (condition->flags == PACH_CONDITION_ADD_SOURCE) {
                result = pach_typed_add(&source, &value);
            } else {
                result = pach_typed_subtract(&source, &value);
            }

            if (result < 0) {
                return result;
            }

            continue;
        }

        {
            int raw_met;
            int linked_met;
            int condition_met;
            int final_condition = index + 1u == segment->end;

            result = pach_evaluate_raw_condition(
                engine,
                condition,
                index,
                source.valid ? &source : NULL,
                &raw_met
            );

            source.valid = 0;

            if (result < 0) {
                return result;
            }

            if (!have_result) {
                linked_met = raw_met;
            } else if (previous_link == PACH_CONDITION_AND_NEXT) {
                linked_met = combined_result && raw_met;
            } else if (previous_link == PACH_CONDITION_OR_NEXT) {
                linked_met = combined_result || raw_met;
            } else {
                return PACH_ACHIEVEMENT_ERROR_CHAIN;
            }

            /*
             * AndNext/OrNext associate the accumulated expression with the
             * following condition. A hit target on that condition therefore
             * counts the whole expression, not just the final raw comparison.
             */
            if (hit_modifier_mode && final_condition) {
                result = pach_apply_hit_modifier(
                    &engine->condition_runtime[index],
                    condition->hit_target,
                    linked_met,
                    stored_hits
                );

                condition_met = linked_met;
            } else {
                result = pach_apply_hit_target(
                    &engine->condition_runtime[index],
                    condition->hit_target,
                    linked_met,
                    final_condition ? hit_adjustment : 0,
                    &condition_met
                );
            }

            if (result < 0) {
                return result;
            }

            combined_result = condition_met;
            have_result = 1;
            previous_link = condition->flags;
        }
    }

    if (!have_result || pach_condition_is_link(previous_link)) {
        return PACH_ACHIEVEMENT_ERROR_CHAIN;
    }

    *segment_met = combined_result;
    return PACH_MEMORY_OK;
}

static void pach_reset_segment_hits(
    PachAchievementEngine *engine,
    const PachConditionSegment *segment)
{
    u32 index;

    if (engine == NULL || segment == NULL) {
        return;
    }

    for (index = segment->start; index < segment->end; ++index) {
        if (!pach_condition_is_modifier(
                engine->conditions[index].flags)) {

            engine->condition_runtime[index].current_hits = 0;
        }
    }
}

static void pach_reset_achievement_hits(
    PachAchievementEngine *engine,
    const PachAchievementDefinition *definition)
{
    u32 group_offset;

    if (engine == NULL || definition == NULL) {
        return;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        const PachAchievementGroup *group =
            &engine->groups[definition->first_group + group_offset];

        u32 condition_offset;

        for (condition_offset = 0;
             condition_offset < group->condition_count;
             ++condition_offset) {

            engine->condition_runtime[
                group->first_condition + condition_offset
            ].current_hits = 0;
        }
    }
}

static int pach_group_pass(
    PachAchievementEngine *engine,
    const PachAchievementGroup *group,
    PachEvaluationPass pass,
    int *pass_result,
    int *required_met,
    int *trigger_exists,
    int *trigger_met)
{
    u32 group_end;
    u32 cursor;
    s32 hit_adjustment = 0;
    int hit_chain_active = 0;
    int result;

    if (engine == NULL || group == NULL || pass_result == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    group_end = group->first_condition + group->condition_count;
    cursor = group->first_condition;
    *pass_result = 0;

    while (cursor < group_end) {
        PachConditionSegment segment;
        int segment_met;
        int evaluate = 0;

        result = pach_find_segment(
            engine,
            group,
            cursor,
            &segment
        );

        if (result < 0) {
            return result;
        }

        if (pass == PACH_PASS_NORMAL &&
            pach_condition_is_hit_modifier(segment.category)) {

            u16 stored_hits = 0;

            result = pach_evaluate_segment(
                engine,
                &segment,
                0,
                1,
                &segment_met,
                &stored_hits
            );

            if (result < 0) {
                return result;
            }

            if (segment.category == PACH_CONDITION_ADD_HITS) {
                hit_adjustment += (s32)stored_hits;
            } else {
                hit_adjustment -= (s32)stored_hits;
            }

            hit_chain_active = 1;
            cursor = segment.end;
            continue;
        }

        switch (pass) {
        case PACH_PASS_PAUSE:
            evaluate =
                segment.category == PACH_CONDITION_PAUSE_IF;
            break;

        case PACH_PASS_RESET:
            evaluate =
                segment.category == PACH_CONDITION_RESET_IF;
            break;

        case PACH_PASS_RESET_NEXT:
            evaluate =
                segment.category == PACH_CONDITION_RESET_NEXT_IF;
            break;

        case PACH_PASS_NORMAL:
            evaluate =
                segment.category == PACH_CONDITION_STANDARD ||
                segment.category == PACH_CONDITION_MEASURED ||
                segment.category == PACH_CONDITION_MEASURED_IF ||
                segment.category == PACH_CONDITION_TRIGGER;
            break;

        default:
            return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
        }

        if (pass == PACH_PASS_NORMAL &&
            hit_chain_active && !evaluate) {

            return PACH_ACHIEVEMENT_ERROR_CHAIN;
        }

        if (evaluate) {
            result = pach_evaluate_segment(
                engine,
                &segment,
                pass == PACH_PASS_NORMAL ? hit_adjustment : 0,
                0,
                &segment_met,
                NULL
            );

            if (result < 0) {
                return result;
            }

            if (pass == PACH_PASS_NORMAL) {
                hit_adjustment = 0;
                hit_chain_active = 0;
            }

            if (pass == PACH_PASS_PAUSE) {
                if (segment_met) {
                    *pass_result = 1;
                    return PACH_MEMORY_OK;
                }
            } else if (pass == PACH_PASS_RESET) {
                if (segment_met) {
                    *pass_result = 1;
                }
            } else if (pass == PACH_PASS_RESET_NEXT) {
                if (segment_met && segment.end < group_end) {
                    PachConditionSegment next_segment;

                    result = pach_find_segment(
                        engine,
                        group,
                        segment.end,
                        &next_segment
                    );

                    if (result < 0) {
                        return result;
                    }

                    pach_reset_segment_hits(
                        engine,
                        &next_segment
                    );
                }
            } else if (segment.category == PACH_CONDITION_TRIGGER) {
                if (trigger_exists == NULL || trigger_met == NULL) {
                    return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
                }

                *trigger_exists = 1;
                *trigger_met = *trigger_met && segment_met;
            } else {
                if (required_met == NULL) {
                    return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
                }

                *required_met = *required_met && segment_met;
            }
        }

        cursor = segment.end;
    }

    if (pass == PACH_PASS_NORMAL && hit_chain_active) {
        return PACH_ACHIEVEMENT_ERROR_CHAIN;
    }

    return PACH_MEMORY_OK;
}

static int pach_evaluate_achievement(
    PachAchievementEngine *engine,
    const PachAchievementDefinition *definition,
    int *achievement_met)
{
    u32 group_offset;
    int any_reset = 0;
    int result;

    if (engine == NULL || definition == NULL || achievement_met == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    *achievement_met = 0;

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index = definition->first_group + group_offset;
        int paused = 0;
        int unused_required = 1;
        int unused_trigger_exists = 0;
        int unused_trigger_met = 1;

        result = pach_group_pass(
            engine,
            &engine->groups[group_index],
            PACH_PASS_PAUSE,
            &paused,
            &unused_required,
            &unused_trigger_exists,
            &unused_trigger_met
        );

        if (result < 0) {
            return result;
        }

        engine->group_paused[group_index] = (u8)paused;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index = definition->first_group + group_offset;
        int reset = 0;
        int unused_required = 1;
        int unused_trigger_exists = 0;
        int unused_trigger_met = 1;

        if (engine->group_paused[group_index]) {
            continue;
        }

        result = pach_group_pass(
            engine,
            &engine->groups[group_index],
            PACH_PASS_RESET,
            &reset,
            &unused_required,
            &unused_trigger_exists,
            &unused_trigger_met
        );

        if (result < 0) {
            return result;
        }

        any_reset = any_reset || reset;
    }

    if (any_reset) {
        pach_reset_achievement_hits(engine, definition);
        return PACH_MEMORY_OK;
    }

    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        u32 group_index = definition->first_group + group_offset;
        int unused_result = 0;
        int unused_required = 1;
        int unused_trigger_exists = 0;
        int unused_trigger_met = 1;

        if (engine->group_paused[group_index]) {
            continue;
        }

        result = pach_group_pass(
            engine,
            &engine->groups[group_index],
            PACH_PASS_RESET_NEXT,
            &unused_result,
            &unused_required,
            &unused_trigger_exists,
            &unused_trigger_met
        );

        if (result < 0) {
            return result;
        }
    }

    {
        int core_met = 0;
        int alternative_met = definition->group_count <= 1u;

        for (group_offset = 0;
             group_offset < definition->group_count;
             ++group_offset) {

            u32 group_index = definition->first_group + group_offset;
            int group_met;
            int required_met = 1;
            int trigger_exists = 0;
            int trigger_met = 1;
            int unused_result = 0;

            if (engine->group_paused[group_index]) {
                group_met = 0;
            } else {
                result = pach_group_pass(
                    engine,
                    &engine->groups[group_index],
                    PACH_PASS_NORMAL,
                    &unused_result,
                    &required_met,
                    &trigger_exists,
                    &trigger_met
                );

                if (result < 0) {
                    return result;
                }

                group_met =
                    required_met &&
                    (!trigger_exists || trigger_met);
            }

            if (group_offset == 0) {
                core_met = group_met;
            } else if (group_met) {
                alternative_met = 1;
            }
        }

        *achievement_met = core_met && alternative_met;
    }

    return PACH_MEMORY_OK;
}

void pach_achievement_engine_reset(PachAchievementEngine *engine)
{
    u32 index;

    if (engine == NULL) {
        return;
    }

    engine->definitions = NULL;
    engine->achievement_count = 0;
    engine->groups = NULL;
    engine->group_count = 0;
    engine->conditions = NULL;
    engine->condition_count = 0;
    engine->memory_ref_count = 0;
    engine->sample_generation = 0;
    engine->sample_cache_hits = 0;
    engine->sample_cache_misses = 0;
    engine->scheduler_tick = 0;
    engine->scheduler_burst_ticks = 0;
    engine->scheduler_full_scans = 0;
    engine->scheduler_partial_scans = 0;
    engine->scheduler_peak_conditions = 0;
    engine->scheduler_enabled = 0;
    engine->scheduler_watch_count = 0;
    engine->scheduler_reserved = 0;
    engine->initialized = 0;

    for (index = 0; index < PACH_ACHIEVEMENT_MAX; ++index) {
        engine->runtime[index].unlocked = 0;
        engine->runtime[index].waiting = 1;
        engine->runtime[index].reserved = 0;
        engine->schedule_period[index] = 1u;
        engine->schedule_phase[index] = 0u;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_CONDITION_MAX;
         ++index) {

        engine->condition_runtime[index].current_hits = 0;
        engine->condition_runtime[index].reserved = 0;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_OPERAND_MAX;
         ++index) {

        engine->operand_runtime[index].effective_address = 0;
        engine->operand_runtime[index].current_value = 0;
        engine->operand_runtime[index].previous_value = 0;
        engine->operand_runtime[index].prior_value = 0;
        engine->operand_runtime[index].initialized = 0;
        engine->operand_runtime[index].reserved[0] = 0;
        engine->operand_runtime[index].reserved[1] = 0;
        engine->operand_runtime[index].reserved[2] = 0;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_GROUP_MAX;
         ++index) {

        engine->group_paused[index] = 0;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_SAMPLE_CACHE_MAX;
         ++index) {

        engine->sample_cache[index].effective_address = 0;
        engine->sample_cache[index].value = 0;
        engine->sample_cache[index].generation = 0;
        engine->sample_cache[index].operand_type = 0;
        engine->sample_cache[index].reserved[0] = 0;
        engine->sample_cache[index].reserved[1] = 0;
        engine->sample_cache[index].reserved[2] = 0;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_SCHEDULER_WATCH_MAX;
         ++index) {

        engine->scheduler_watches[index].effective_address = 0;
        engine->scheduler_watches[index].value = 0;
        engine->scheduler_watches[index].occurrences = 0;
        engine->scheduler_watches[index].operand_type = 0;
        engine->scheduler_watches[index].initialized = 0;
    }
}

int pach_achievement_engine_init(
    PachAchievementEngine *engine,
    PachAchievementDefinition *definitions,
    u32 achievement_count,
    PachAchievementGroup *groups,
    u32 group_count,
    PachAchievementCondition *conditions,
    u32 condition_count)
{
    u32 index;

    if (engine == NULL ||
        (achievement_count > 0 && definitions == NULL) ||
        (group_count > 0 && groups == NULL) ||
        (condition_count > 0 && conditions == NULL)) {

        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    pach_achievement_engine_reset(engine);

    if (achievement_count > PACH_ACHIEVEMENT_MAX ||
        group_count > PACH_ACHIEVEMENT_GROUP_MAX ||
        condition_count > PACH_ACHIEVEMENT_CONDITION_MAX) {

        return PACH_ACHIEVEMENT_ERROR_COUNT;
    }

    engine->definitions = definitions;
    engine->achievement_count = achievement_count;
    engine->groups = groups;
    engine->group_count = group_count;
    engine->conditions = conditions;
    engine->condition_count = condition_count;

    for (index = 0; index < condition_count; ++index) {
        const PachAchievementCondition *condition = &conditions[index];

        if (!pach_operand_is_valid(&condition->left) ||
            !pach_operand_is_valid(&condition->right) ||
            condition->flags > PACH_CONDITION_SUB_HITS) {

            pach_achievement_engine_reset(engine);
            return PACH_ACHIEVEMENT_ERROR_CONDITION;
        }

        if (pach_condition_is_modifier(condition->flags)) {
            if (condition->comparison != PACH_COMPARE_NONE ||
                condition->hit_target != 0) {

                pach_achievement_engine_reset(engine);
                return PACH_ACHIEVEMENT_ERROR_CONDITION;
            }
        } else if (condition->comparison >
                   PACH_COMPARE_GREATER_EQUAL) {

            pach_achievement_engine_reset(engine);
            return PACH_ACHIEVEMENT_ERROR_COMPARISON;
        }

        if (condition->flags == PACH_CONDITION_ADD_ADDRESS &&
            pach_operand_is_float(condition->left.type)) {

            pach_achievement_engine_reset(engine);
            return PACH_ACHIEVEMENT_ERROR_OPERAND;
        }

        if (pach_operand_is_memory(condition->left.type)) {
            ++engine->memory_ref_count;
        }

        if (pach_operand_is_memory(condition->right.type)) {
            ++engine->memory_ref_count;
        }
    }

    for (index = 0; index < group_count; ++index) {
        const PachAchievementGroup *group = &groups[index];
        u32 cursor = group->first_condition;
        u32 end = cursor + group->condition_count;
        int hit_chain_active = 0;

        while (cursor < end) {
            PachConditionSegment segment;
            int result = pach_find_segment(
                engine,
                group,
                cursor,
                &segment
            );

            if (result < 0) {
                pach_achievement_engine_reset(engine);
                return result;
            }

            if (segment.category == PACH_CONDITION_ADD_SOURCE ||
                segment.category == PACH_CONDITION_SUB_SOURCE ||
                segment.category == PACH_CONDITION_ADD_ADDRESS ||
                pach_condition_is_link(segment.category)) {

                pach_achievement_engine_reset(engine);
                return PACH_ACHIEVEMENT_ERROR_CHAIN;
            }

            if (pach_condition_is_hit_modifier(segment.category)) {
                hit_chain_active = 1;
            } else {
                hit_chain_active = 0;
            }

            cursor = segment.end;
        }

        if (hit_chain_active) {
            pach_achievement_engine_reset(engine);
            return PACH_ACHIEVEMENT_ERROR_CHAIN;
        }
    }

    {
        int scheduler_result =
            pach_scheduler_initialize(
                engine,
                PACH_ACHIEVEMENT_SCHEDULER_AUTO
            );

        if (scheduler_result < 0) {
            pach_achievement_engine_reset(engine);
            return scheduler_result;
        }
    }

    engine->initialized = 1;
    return PACH_MEMORY_OK;
}

static void pach_achievement_progress_reset(
    PachAchievementProgress *progress)
{
    if (progress == NULL) {
        return;
    }

    progress->current = 0;
    progress->target = 0;
    progress->available = 0;
    progress->measured = 0;
    progress->reserved = 0;
}

static int pach_achievement_progress_from_hits(
    const PachAchievementEngine *engine,
    u32 condition_index,
    const PachAchievementCondition *condition,
    int measured,
    PachAchievementProgress *progress)
{
    u32 current;
    u32 target;

    if (engine == NULL || condition == NULL || progress == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    target = condition->hit_target;

    if (target == 0) {
        return 0;
    }

    current = engine->condition_runtime[
        condition_index
    ].current_hits;

    if (current > target) {
        current = target;
    }

    progress->current = current;
    progress->target = target;
    progress->available = 1;
    progress->measured = measured ? 1u : 0u;

    return 1;
}

int pach_achievement_engine_get_progress(
    const PachAchievementEngine *engine,
    u32 achievement_index,
    PachAchievementProgress *progress)
{
    const PachAchievementDefinition *definition;
    u32 group_offset;

    if (engine == NULL || progress == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    pach_achievement_progress_reset(progress);

    if (!engine->initialized ||
        achievement_index >= engine->achievement_count) {

        return 0;
    }

    definition = &engine->definitions[achievement_index];

    /*
     * Prefer an explicit Measured hit target. The evaluator already stores
     * the exact RA hit count after AddHits/SubHits and linked-condition
     * processing, so this is the most reliable form of live progress.
     */
    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        const PachAchievementGroup *group = &engine->groups[
            definition->first_group + group_offset
        ];

        u32 local_index;

        for (local_index = 0;
             local_index < group->condition_count;
             ++local_index) {

            u32 condition_index =
                group->first_condition + local_index;

            const PachAchievementCondition *condition =
                &engine->conditions[condition_index];

            if (condition->flags == PACH_CONDITION_MEASURED &&
                condition->hit_target > 0) {

                int result = pach_achievement_progress_from_hits(
                    engine,
                    condition_index,
                    condition,
                    1,
                    progress
                );

                if (result > 0 &&
                    engine->runtime[achievement_index].unlocked) {

                    progress->current = progress->target;
                }

                return result;
            }
        }
    }

    /*
     * A direct integer Measured comparison can expose its sampled value.
     * Accumulated AddSource and indirect expressions require a fresh full
     * evaluation and are intentionally hidden rather than approximated.
     */
    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        const PachAchievementGroup *group = &engine->groups[
            definition->first_group + group_offset
        ];

        u32 local_index;
        int source_active = 0;

        for (local_index = 0;
             local_index < group->condition_count;
             ++local_index) {

            u32 condition_index =
                group->first_condition + local_index;

            const PachAchievementCondition *condition =
                &engine->conditions[condition_index];

            if (condition->flags == PACH_CONDITION_ADD_SOURCE ||
                condition->flags == PACH_CONDITION_SUB_SOURCE ||
                condition->flags == PACH_CONDITION_ADD_ADDRESS) {

                source_active = 1;
                continue;
            }

            if (condition->flags == PACH_CONDITION_MEASURED &&
                condition->hit_target == 0 &&
                !source_active &&
                pach_operand_is_memory(condition->left.type) &&
                !pach_operand_is_float(condition->left.type) &&
                condition->right.type == PACH_OPERAND_VALUE_U32 &&
                (condition->comparison == PACH_COMPARE_EQUAL ||
                 condition->comparison == PACH_COMPARE_GREATER_EQUAL ||
                 condition->comparison == PACH_COMPARE_GREATER)) {

                u32 current;
                u32 target = condition->right.value;

                if (condition->comparison == PACH_COMPARE_GREATER &&
                    target < 0xFFFFFFFFu) {

                    ++target;
                }

                if (target > 0 &&
                    pach_read_operand_bits(
                        engine,
                        &condition->left,
                        condition_index,
                        0,
                        &current
                    ) >= 0) {

                    if (current > target) {
                        current = target;
                    }

                    if (engine->runtime[
                            achievement_index
                        ].unlocked) {

                        current = target;
                    }

                    progress->current = current;
                    progress->target = target;
                    progress->available = 1;
                    progress->measured = 1;
                    return 1;
                }
            }

            source_active = 0;
        }
    }

    /*
     * Some older sets use a normal hit target without a Measured flag. It
     * still represents deterministic progress, so expose it as a fallback
     * when the target is larger than one.
     */
    for (group_offset = 0;
         group_offset < definition->group_count;
         ++group_offset) {

        const PachAchievementGroup *group = &engine->groups[
            definition->first_group + group_offset
        ];

        u32 local_index;

        for (local_index = 0;
             local_index < group->condition_count;
             ++local_index) {

            u32 condition_index =
                group->first_condition + local_index;

            const PachAchievementCondition *condition =
                &engine->conditions[condition_index];

            if (condition->hit_target > 1u &&
                !pach_condition_is_modifier(condition->flags)) {

                int result = pach_achievement_progress_from_hits(
                    engine,
                    condition_index,
                    condition,
                    0,
                    progress
                );

                if (result > 0 &&
                    engine->runtime[achievement_index].unlocked) {

                    progress->current = progress->target;
                }

                return result;
            }
        }
    }

    return 0;
}

int pach_achievement_engine_configure_scheduler(
    PachAchievementEngine *engine,
    u8 mode)
{
    if (engine == NULL ||
        !engine->initialized ||
        mode > PACH_ACHIEVEMENT_SCHEDULER_ADAPTIVE) {

        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    return pach_scheduler_initialize(
        engine,
        mode
    );
}

int pach_achievement_engine_restore_ids(
    PachAchievementEngine *engine,
    const u32 *achievement_ids,
    u32 achievement_id_count,
    u32 *restored_count)
{
    u32 id_index;
    u32 definition_index;

    if (engine == NULL || restored_count == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    *restored_count = 0;

    if (!engine->initialized ||
        (achievement_id_count > 0 && achievement_ids == NULL)) {

        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    for (id_index = 0; id_index < achievement_id_count; ++id_index) {
        for (definition_index = 0;
             definition_index < engine->achievement_count;
             ++definition_index) {

            if (engine->definitions[definition_index].id !=
                achievement_ids[id_index]) {

                continue;
            }

            if (!engine->runtime[definition_index].unlocked) {
                engine->runtime[definition_index].unlocked = 1;
                engine->runtime[definition_index].waiting = 0;
                ++(*restored_count);
            }

            break;
        }
    }

    return PACH_MEMORY_OK;
}

int pach_achievement_engine_tick(
    PachAchievementEngine *engine,
    const PachMemoryMap *memory_map,
    PachAchievementEvent *events,
    u32 event_capacity,
    u32 *event_count)
{
    u8 active_groups[PACH_ACHIEVEMENT_GROUP_MAX];
    u32 index;
    int watch_changed = 0;
    int full_scan;
    int result;

    if (engine == NULL || memory_map == NULL || event_count == NULL) {
        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    *event_count = 0;

    if (!engine->initialized ||
        (event_capacity > 0 && events == NULL)) {

        return PACH_ACHIEVEMENT_ERROR_ARGUMENT;
    }

    pach_sample_cache_begin(engine);

    if (engine->scheduler_enabled &&
        engine->scheduler_watch_count > 0) {

        result = pach_scheduler_update_watches(
            engine,
            memory_map,
            &watch_changed
        );

        if (result < 0) {
            return result;
        }

        if (watch_changed) {
            engine->scheduler_burst_ticks =
                engine->condition_count >
                    PACH_SCHEDULER_EXTREME_CONDITION_THRESHOLD
                ? 1u
                : PACH_SCHEDULER_BURST_TICKS;
        }
    }

    full_scan =
        !engine->scheduler_enabled ||
        engine->scheduler_burst_ticks > 0;

    if (full_scan) {
        ++engine->scheduler_full_scans;
    } else {
        ++engine->scheduler_partial_scans;
    }

    for (index = 0;
         index < PACH_ACHIEVEMENT_GROUP_MAX;
         ++index) {

        active_groups[index] = 0;
    }

    for (index = 0;
         index < engine->achievement_count;
         ++index) {

        const PachAchievementDefinition *definition;
        u32 group_offset;

        if (engine->runtime[index].unlocked ||
            !pach_scheduler_achievement_due(
                engine,
                index,
                full_scan
            )) {

            continue;
        }

        definition = &engine->definitions[index];

        for (group_offset = 0;
             group_offset < definition->group_count;
             ++group_offset) {

            u32 group_index =
                (u32)definition->first_group +
                group_offset;

            if (group_index >= engine->group_count) {
                return PACH_ACHIEVEMENT_ERROR_GROUP;
            }

            active_groups[group_index] = 1;
        }
    }

    for (index = 0; index < engine->group_count; ++index) {
        if (!active_groups[index]) {
            continue;
        }

        result = pach_sample_group(
            engine,
            &engine->groups[index],
            memory_map
        );

        if (result < 0) {
            return result;
        }
    }

    for (index = 0; index < engine->achievement_count; ++index) {
        PachAchievementDefinition *definition;
        PachAchievementRuntime *runtime;
        int achievement_met;

        runtime = &engine->runtime[index];

        if (runtime->unlocked ||
            !pach_scheduler_achievement_due(
                engine,
                index,
                full_scan
            )) {

            continue;
        }

        definition = &engine->definitions[index];

        result = pach_evaluate_achievement(
            engine,
            definition,
            &achievement_met
        );

        if (result < 0) {
            return result;
        }

        if (runtime->waiting) {
            if (!achievement_met) {
                runtime->waiting = 0;
            } else {
                pach_reset_achievement_hits(
                    engine,
                    definition
                );
            }

            continue;
        }

        if (!achievement_met) {
            continue;
        }

        if (*event_count >= event_capacity) {
            return PACH_ACHIEVEMENT_ERROR_EVENTS;
        }

        runtime->unlocked = 1;
        events[*event_count].achievement_index = index;
        events[*event_count].achievement_id = definition->id;
        ++(*event_count);
    }

    if (engine->scheduler_burst_ticks > 0) {
        --engine->scheduler_burst_ticks;
    }

    ++engine->scheduler_tick;

    if (engine->scheduler_tick >=
        PACH_ACHIEVEMENT_SCHEDULER_CYCLE) {

        engine->scheduler_tick = 0;
    }

    return *event_count > 0
        ? PACH_ACHIEVEMENT_TICK_UNLOCKED
        : PACH_ACHIEVEMENT_TICK_IDLE;
}
