#ifndef PACH_NOTIFICATION_H
#define PACH_NOTIFICATION_H

#include <pspthreadman.h>
#include <psptypes.h>

#include "pach_achievement.h"

#define PACH_NOTIFICATION_QUEUE_CAPACITY 6u

typedef enum PachNotificationKind {
    PACH_NOTIFICATION_KIND_ACHIEVEMENT = 0,
    PACH_NOTIFICATION_KIND_STATUS = 1
} PachNotificationKind;

typedef struct PachNotification {
    u32 achievement_id;
    u16 points;
    u8 type;
    u8 kind;
    char title[PACH_ACHIEVEMENT_TITLE_CAPACITY];
} PachNotification;

typedef struct PachNotificationQueue {
    PachNotification items[PACH_NOTIFICATION_QUEUE_CAPACITY];

    u32 head;
    u32 tail;
    u32 count;

    SceUID mutex_id;
    int initialized;
} PachNotificationQueue;

enum PachNotificationResult {
    PACH_NOTIFICATION_OK = 0,
    PACH_NOTIFICATION_EMPTY = 1,

    PACH_NOTIFICATION_ERROR_ARGUMENT = -8001,
    PACH_NOTIFICATION_ERROR_CREATE   = -8002,
    PACH_NOTIFICATION_ERROR_LOCK     = -8003,
    PACH_NOTIFICATION_ERROR_FULL     = -8004,
    PACH_NOTIFICATION_ERROR_TITLE    = -8005
};

void pach_notification_queue_reset(
    PachNotificationQueue *queue
);

int pach_notification_queue_init(
    PachNotificationQueue *queue
);

void pach_notification_queue_shutdown(
    PachNotificationQueue *queue
);

int pach_notification_enqueue(
    PachNotificationQueue *queue,
    u32 achievement_id,
    u16 points,
    u8 type,
    const char *title
);

int pach_notification_enqueue_status(
    PachNotificationQueue *queue,
    u16 achievement_count
);

int pach_notification_dequeue(
    PachNotificationQueue *queue,
    PachNotification *notification
);

#endif
