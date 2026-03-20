#include <string.h>
#include "rcheevos_glue.h"
#include "memory.h"

/* ============================================================
 * STRING HELPERS
 * ============================================================ */
static int str_starts(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}

/* Custom simple float parser because KERNEL_LIBC has no strtod */
static float pach_strtof(const char *s, const char **endptr)
{
    float result = 0.0f;
    float fraction = 1.0f;
    int sign = 1;

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    /* Integer part */
    while (*s >= '0' && *s <= '9') {
        result = result * 10.0f + (*s - '0');
        s++;
    }

    /* Fractional part */
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            fraction *= 0.1f;
            result += (*s - '0') * fraction;
            s++;
        }
    }

    if (endptr) *endptr = s;
    return result * sign;
}

/* Custom strtoul */
static unsigned int pach_strtoul(const char *s, const char **endptr, int base)
{
    unsigned int res = 0;
    if (base == 16) {
        while (*s) {
            if (*s >= '0' && *s <= '9') res = res * 16 + (*s - '0');
            else if (*s >= 'a' && *s <= 'f') res = res * 16 + (*s - 'a' + 10);
            else if (*s >= 'A' && *s <= 'F') res = res * 16 + (*s - 'A' + 10);
            else break;
            s++;
        }
    } else {
        while (*s >= '0' && *s <= '9') {
            res = res * 10 + (*s - '0');
            s++;
        }
    }
    if (endptr) *endptr = s;
    return res;
}

/* ============================================================
 * PARSE OPERAND
 * ============================================================ */
static int parse_operand(const char *s, RC_Operand *op, const char **end)
{
    memset(op, 0, sizeof(*op));
    const char *p = s;

    if (*p == 'd') { op->is_delta = 1; p++; }
    else if (*p == 'p') { op->is_prior = 1; p++; }

    if (str_starts(p, "0xX")) {
        op->size = RC_MEMSIZE_32BIT;
        op->address = pach_strtoul(p + 3, end, 16);
        return 1;
    }
    if (str_starts(p, "0xH")) {
        op->size = RC_MEMSIZE_8BIT;
        op->address = pach_strtoul(p + 3, end, 16);
        return 1;
    }
    if (str_starts(p, "0xM")) {
        op->size = RC_MEMSIZE_BIT0;
        op->address = pach_strtoul(p + 3, end, 16);
        return 1;
    }
    if (str_starts(p, "0x")) {
        op->size = RC_MEMSIZE_16BIT;
        op->address = pach_strtoul(p + 2, end, 16);
        return 1;
    }
    if (str_starts(p, "fF")) {
        op->size = RC_MEMSIZE_FLOAT_BE;
        op->address = pach_strtoul(p + 2, end, 16);
        return 1;
    }
    if (*p == 'f' && !op->is_delta && !op->is_prior) {
        op->size = RC_MEMSIZE_CONST_FLOAT;
        op->const_float = pach_strtof(p + 1, end);
        op->is_delta = 0;
        op->is_prior = 0;
        return 1;
    }

    op->size = RC_MEMSIZE_CONST_UINT;
    op->is_delta = 0;
    op->is_prior = 0;
    op->const_uint = pach_strtoul(p, end, 10);
    return 1;
}

/* ============================================================
 * PARSE COMPARISON OPERATOR
 * ============================================================ */
static RC_CompOp parse_comp_op(const char **ps)
{
    const char *s = *ps;
    if (s[0] == '>' && s[1] == '=') { *ps = s + 2; return RC_OP_GE; }
    if (s[0] == '<' && s[1] == '=') { *ps = s + 2; return RC_OP_LE; }
    if (s[0] == '!' && s[1] == '=') { *ps = s + 2; return RC_OP_NE; }
    if (s[0] == '=')                { *ps = s + 1; return RC_OP_EQ; }
    if (s[0] == '>')                { *ps = s + 1; return RC_OP_GT; }
    if (s[0] == '<')                { *ps = s + 1; return RC_OP_LT; }
    return RC_OP_NONE;
}

/* ============================================================
 * PARSE CONDITION PREFIX
 * ============================================================ */
static RC_CondType parse_cond_prefix(const char **ps)
{
    const char *s = *ps;
    if (s[0] && s[1] == ':') {
        RC_CondType type = RC_COND_STANDARD;
        switch (s[0]) {
            case 'R': type = RC_COND_RESET_IF;      break;
            case 'P': type = RC_COND_PAUSE_IF;      break;
            case 'N': type = RC_COND_AND_NEXT;      break;
            case 'O': type = RC_COND_OR_NEXT;       break;
            case 'T': type = RC_COND_TRIGGER;       break;
            case 'M': type = RC_COND_MEASURED;      break;
            case 'Q': type = RC_COND_MEASURED_IF;    break;
            case 'A': type = RC_COND_ADD_SOURCE;     break;
            case 'I': type = RC_COND_ADD_ADDRESS;    break;
            case 'Z': type = RC_COND_RESET_NEXT_IF; break;
            case 'B': type = RC_COND_SUB_SOURCE;     break;
            default:  return RC_COND_STANDARD;
        }
        *ps = s + 2;
        return type;
    }
    return RC_COND_STANDARD;
}

static const char *find_comp_op(const char *s)
{
    while (*s) {
        if (*s == '=' || *s == '>' || *s == '<') return s;
        if (*s == '!' && s[1] == '=') return s;
        s++;
    }
    return NULL;
}

static int parse_single_condition(const char *s, int len, RC_Condition *out)
{
    char buf[512];
    const char *p, *op_pos, *after_op, *end_ptr;

    memset(out, 0, sizeof(*out));
    if (len <= 0 || len >= (int)sizeof(buf)) return 0;

    memcpy(buf, s, len);
    buf[len] = '\0';
    p = buf;

    out->type = parse_cond_prefix(&p);
    op_pos = find_comp_op(p);
    if (!op_pos) return 0;

    {
        char leftbuf[256];
        int leftlen = (int)(op_pos - p);
        if (leftlen <= 0 || leftlen >= (int)sizeof(leftbuf)) return 0;
        memcpy(leftbuf, p, leftlen);
        leftbuf[leftlen] = '\0';
        if (!parse_operand(leftbuf, &out->left, &end_ptr)) return 0;
    }

    after_op = op_pos;
    out->op = parse_comp_op(&after_op);
    if (out->op == RC_OP_NONE) return 0;

    {
        char rightbuf[256];
        int rlen = (int)strlen(after_op);
        if (rlen <= 0 || rlen >= (int)sizeof(rightbuf)) return 0;
        memcpy(rightbuf, after_op, rlen);
        rightbuf[rlen] = '\0';

        out->required_hits = 0;

        {
            char *last_dot = NULL, *second_dot = NULL;
            int i;
            for (i = rlen - 1; i >= 0; i--) {
                if (rightbuf[i] == '.') { last_dot = &rightbuf[i]; break; }
            }
            if (last_dot && last_dot == &rightbuf[rlen - 1]) {
                for (i = (int)(last_dot - rightbuf) - 1; i >= 0; i--) {
                    if (rightbuf[i] == '.') { second_dot = &rightbuf[i]; break; }
                }
                if (second_dot) {
                    int all_digits = 1;
                    char *dp;
                    for (dp = second_dot + 1; dp < last_dot; dp++) {
                        if (*dp < '0' || *dp > '9') { all_digits = 0; break; }
                    }
                    if (all_digits && (last_dot - second_dot) > 1) {
                        out->required_hits = pach_strtoul(second_dot + 1, NULL, 10);
                        *second_dot = '\0';
                    }
                }
            }
        }

        if (!parse_operand(rightbuf, &out->right, &end_ptr)) return 0;
    }

    out->current_hits = 0;
    return 1;
}

int rc_glue_parse(const char *logic, RC_ParsedAchievement *out)
{
    const char *p;
    int group_idx;

    memset(out, 0, sizeof(*out));
    out->is_active = 1;
    if (!logic || !logic[0]) return 0;

    p = logic;
    group_idx = 0;

    while (*p && group_idx < RC_MAX_GROUPS) {
        RC_CondGroup *grp = &out->groups[group_idx];
        grp->count = 0;

        while (*p) {
            const char *start = p;
            int len = 0;

            while (*p && *p != '_' && *p != 'S') { p++; len++; }

            if (len > 0 && grp->count < RC_MAX_CONDITIONS) {
                if (parse_single_condition(start, len, &grp->conds[grp->count]))
                    grp->count++;
            }

            if (*p == '_') { p++; }
            else if (*p == 'S') { p++; break; }
        }

        if (grp->count > 0) group_idx++;
    }

    out->num_groups = group_idx;
    return (group_idx > 0) ? 1 : 0;
}

int rc_glue_parse_all(PACH_LoadedGame *game,
                      RC_ParsedAchievement *out_array, int max_count)
{
    int count = 0;
    if (!game || !game->loaded || !out_array) return 0;

    for (int i = 0; i < game->header.num_achievements && i < max_count; i++) {
        if (rc_glue_parse(game->achievements[i].ra_logic, &out_array[i]))
            count++;
        else
            memset(&out_array[i], 0, sizeof(RC_ParsedAchievement));
    }
    return count;
}

static RC_DeltaSlot *find_or_create_slot(RC_RuntimeState *state,
                                          unsigned int addr, RC_MemSize size)
{
    for (int i = 0; i < state->num_slots; i++) {
        if (state->slots[i].addr == addr && state->slots[i].size == size)
            return &state->slots[i];
    }
    if (state->num_slots < RC_MAX_DELTA_SLOTS) {
        RC_DeltaSlot *slot = &state->slots[state->num_slots];
        memset(slot, 0, sizeof(*slot));
        slot->addr = addr;
        slot->size = size;
        state->num_slots++;
        return slot;
    }
    return NULL;
}

static float read_mem_value(unsigned int addr, RC_MemSize size)
{
    switch (size) {
        case RC_MEMSIZE_8BIT:     return (float)pach_mem_read8(addr);
        case RC_MEMSIZE_16BIT:    return (float)pach_mem_read16(addr);
        case RC_MEMSIZE_32BIT:    return (float)pach_mem_read32(addr);
        case RC_MEMSIZE_BIT0:     return (float)pach_mem_read_bit0(addr);
        case RC_MEMSIZE_FLOAT_BE: return pach_mem_read_float_be(addr);
        default:                  return 0.0f;
    }
}

static float resolve_operand(RC_Operand *op, RC_RuntimeState *state,
                              unsigned int indirect_offset)
{
    unsigned int addr;
    RC_DeltaSlot *slot;

    if (op->size == RC_MEMSIZE_CONST_UINT)  return (float)op->const_uint;
    if (op->size == RC_MEMSIZE_CONST_FLOAT) return op->const_float;

    addr = op->address + indirect_offset;

    if (op->is_delta || op->is_prior) {
        slot = find_or_create_slot(state, addr, op->size);
        if (!slot) return 0.0f;
        return op->is_delta ? slot->delta : slot->prior;
    }

    return read_mem_value(addr, op->size);
}

static int compare_floats(float l, RC_CompOp op, float r)
{
    switch (op) {
        case RC_OP_EQ: { float d = l - r; if (d < 0) d = -d; return d < 0.001f; }
        case RC_OP_NE: { float d = l - r; if (d < 0) d = -d; return d >= 0.001f; }
        case RC_OP_GT: return l > r;
        case RC_OP_LT: return l < r;
        case RC_OP_GE: return l >= r;
        case RC_OP_LE: return l <= r;
        default: return 0;
    }
}

static void update_delta_snapshots(RC_RuntimeState *state)
{
    for (int i = 0; i < state->num_slots; i++) {
        RC_DeltaSlot *slot = &state->slots[i];
        float current = read_mem_value(slot->addr, slot->size);
        slot->delta = slot->current;
        if (current != slot->current) slot->prior = slot->current;
        slot->current = current;
    }
}

static void register_operand(RC_Operand *op, RC_RuntimeState *state)
{
    if (op->size == RC_MEMSIZE_CONST_UINT || op->size == RC_MEMSIZE_CONST_FLOAT)
        return;
    RC_DeltaSlot *slot = find_or_create_slot(state, op->address, op->size);
    if (slot) {
        float v = read_mem_value(op->address, op->size);
        slot->current = v;
        slot->delta = v;
        slot->prior = v;
    }
}

static void register_achievement_memrefs(RC_ParsedAchievement *ach,
                                          RC_RuntimeState *state)
{
    for (int g = 0; g < ach->num_groups; g++) {
        RC_CondGroup *grp = &ach->groups[g];
        for (int c = 0; c < grp->count; c++) {
            register_operand(&grp->conds[c].left, state);
            register_operand(&grp->conds[c].right, state);
        }
    }
}

static int evaluate_group(RC_CondGroup *grp, RC_RuntimeState *state,
                           int *out_reset, int *out_paused)
{
    int group_result = 1;
    int paused = 0;
    int has_trigger = 0, trigger_hit = 0;
    float add_source = 0.0f;
    unsigned int add_address = 0;
    int and_next_active = 0, and_next_result = 1;
    int or_next_active = 0, or_next_result = 0;
    int reset_next = 0;

    *out_reset = 0;
    *out_paused = 0;

    for (int i = 0; i < grp->count; i++) {
        RC_Condition *cond = &grp->conds[i];
        if (cond->type == RC_COND_PAUSE_IF) {
            float lv = resolve_operand(&cond->left, state, 0);
            float rv = resolve_operand(&cond->right, state, 0);
            if (compare_floats(lv, cond->op, rv)) {
                if (cond->required_hits > 0) {
                    cond->current_hits++;
                    if (cond->current_hits >= cond->required_hits) paused = 1;
                } else {
                    paused = 1;
                }
            }
        }
    }
    if (paused) { *out_paused = 1; return 0; }

    add_source = 0.0f;
    add_address = 0;

    for (int i = 0; i < grp->count; i++) {
        RC_Condition *cond = &grp->conds[i];
        float lv, rv;
        int cmp_result;

        if (cond->type == RC_COND_PAUSE_IF) continue;

        lv = resolve_operand(&cond->left, state, add_address);
        lv += add_source;
        rv = resolve_operand(&cond->right, state, add_address);

        if (cond->type != RC_COND_ADD_SOURCE &&
            cond->type != RC_COND_SUB_SOURCE &&
            cond->type != RC_COND_ADD_ADDRESS)
        {
            add_source = 0.0f;
            add_address = 0;
        }

        switch (cond->type) {
            case RC_COND_ADD_SOURCE:  add_source = lv; continue;
            case RC_COND_SUB_SOURCE: add_source = -lv; continue;
            case RC_COND_ADD_ADDRESS: add_address = (unsigned int)lv; continue;
            default: break;
        }

        cmp_result = compare_floats(lv, cond->op, rv);

        if (cond->type == RC_COND_RESET_NEXT_IF) { reset_next = cmp_result; continue; }
        if (reset_next) { cond->current_hits = 0; reset_next = 0; }

        if (cond->required_hits > 0) {
            if (cmp_result && cond->current_hits < cond->required_hits)
                cond->current_hits++;
            cmp_result = (cond->current_hits >= cond->required_hits) ? 1 : 0;
        }

        if (cond->type == RC_COND_RESET_IF) {
            if (cmp_result) { *out_reset = 1; return 0; }
            continue;
        }

        if (cond->type == RC_COND_TRIGGER) {
            has_trigger = 1;
            if (cmp_result) trigger_hit = 1;
            else group_result = 0;
            continue;
        }

        if (cond->type == RC_COND_MEASURED_IF) {
            if (!cmp_result) group_result = 0;
            continue;
        }

        if (cond->type == RC_COND_MEASURED) {
            if (!cmp_result) group_result = 0;
            continue;
        }

        if (cond->type == RC_COND_AND_NEXT) {
            if (and_next_active) and_next_result = and_next_result && cmp_result;
            else { and_next_active = 1; and_next_result = cmp_result; }
            continue;
        }

        if (cond->type == RC_COND_OR_NEXT) {
            if (or_next_active) or_next_result = or_next_result || cmp_result;
            else { or_next_active = 1; or_next_result = cmp_result; }
            continue;
        }

        {
            int final = cmp_result;
            if (and_next_active) {
                final = and_next_result && cmp_result;
                and_next_active = 0; and_next_result = 1;
            }
            if (or_next_active) {
                final = or_next_result || cmp_result;
                or_next_active = 0; or_next_result = 0;
            }
            if (!final) group_result = 0;
        }
    }

    if (has_trigger && !trigger_hit) group_result = 0;
    return group_result;
}

static void reset_all_hits(RC_ParsedAchievement *ach)
{
    for (int g = 0; g < ach->num_groups; g++) {
        RC_CondGroup *grp = &ach->groups[g];
        for (int c = 0; c < grp->count; c++)
            grp->conds[c].current_hits = 0;
    }
}

static int evaluate_achievement(RC_ParsedAchievement *ach, RC_RuntimeState *state)
{
    int reset = 0, paused = 0;

    if (!ach->is_active || ach->num_groups == 0) return 0;

    int core = evaluate_group(&ach->groups[0], state, &reset, &paused);
    if (reset) { reset_all_hits(ach); return 0; }
    if (paused) return 0;

    int has_alts = (ach->num_groups > 1) ? 1 : 0;
    int any_alt = 0;

    if (has_alts) {
        for (int g = 1; g < ach->num_groups; g++) {
            int ar = 0, ap = 0;
            int alt = evaluate_group(&ach->groups[g], state, &ar, &ap);
            if (ar) { reset_all_hits(ach); return 0; }
            if (alt && !ap) any_alt = 1;
        }
    }

    return (core && (!has_alts || any_alt)) ? 1 : 0;
}

void rc_glue_init(RC_RuntimeState *state)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

RC_EvalResult rc_glue_update(PACH_LoadedGame *game,
                              PACH_ProfileGameProgress *progress,
                              RC_RuntimeState *state,
                              RC_ParsedAchievement *parsed_cache,
                              int num_parsed)
{
    RC_EvalResult result;
    result.unlocked_index = -1;
    result.unlocked_def = NULL;

    if (!game || !progress || !state || !parsed_cache) return result;
    if (!game->loaded) return result;

    static int refs_registered = 0;
    if (!refs_registered) {
        for (int i = 0; i < num_parsed; i++) {
            if (parsed_cache[i].is_active)
                register_achievement_memrefs(&parsed_cache[i], state);
        }
        refs_registered = 1;
    }

    for (int i = 0; i < game->header.num_achievements && i < num_parsed; i++) {
        if (pach_profile_is_unlocked(progress, i)) continue;
        if (!parsed_cache[i].is_active) continue;

        if (evaluate_achievement(&parsed_cache[i], state)) {
            pach_profile_set_unlocked(progress, i);
            parsed_cache[i].is_active = 0;
            result.unlocked_index = i;
            result.unlocked_def = &game->achievements[i];
            update_delta_snapshots(state);
            return result;
        }
    }

    update_delta_snapshots(state);
    return result;
}