#ifndef PACH_GAME_DETECTOR_H
#define PACH_GAME_DETECTOR_H

#define PACH_GAME_ID_LENGTH   10
#define PACH_GAME_ID_CAPACITY 11

/*
 * Успех: 0.
 * Ошибка: отрицательное значение.
 */
int pach_game_detect(
    char game_id[PACH_GAME_ID_CAPACITY]
);

#endif