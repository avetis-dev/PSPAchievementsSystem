#include "pach_game_context.h"

static void pach_clear_game_id(
    char game_id[PACH_GAME_ID_CAPACITY])
{
    int index;

    if (game_id == NULL) {
        return;
    }

    for (index = 0;
         index < PACH_GAME_ID_CAPACITY;
         ++index) {

        game_id[index] = '\0';
    }
}

void pach_game_context_reset(PachGameContext *context)
{
    if (context == NULL) {
        return;
    }

    pach_clear_game_id(context->game_id);

    pach_memory_map_init(
        &context->memory_map
    );

    context->initialized = 0;
}

int pach_game_context_init(
    PachGameContext *context,
    const char *game_id)
{
    int index;

    if (context == NULL || game_id == NULL) {
        return PACH_GAME_CONTEXT_ERROR_ARGUMENT;
    }

    pach_game_context_reset(context);

    /*
     * DISC_ID обязан содержать ровно 10 символов,
     * после которых идёт нулевой терминатор.
     */
    for (index = 0;
         index < PACH_GAME_ID_LENGTH;
         ++index) {

        if (game_id[index] == '\0') {
            return PACH_GAME_CONTEXT_ERROR_GAME_ID;
        }

        context->game_id[index] =
            game_id[index];
    }

    if (game_id[PACH_GAME_ID_LENGTH] != '\0') {
        pach_game_context_reset(context);
        return PACH_GAME_CONTEXT_ERROR_GAME_ID;
    }

    context->game_id[PACH_GAME_ID_LENGTH] = '\0';

    pach_memory_map_init(
        &context->memory_map
    );

    context->initialized = 1;

    return PACH_GAME_CONTEXT_OK;
}