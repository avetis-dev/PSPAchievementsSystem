#include "pach_notification.h"

#include <stddef.h>

static void pach_notification_clear(
    PachNotification *notification)
{
    u32 index;

    if (notification == NULL) {
        return;
    }

    notification->achievement_id = 0;
    notification->points = 0;
    notification->type = 0;
    notification->kind = PACH_NOTIFICATION_KIND_ACHIEVEMENT;

    for (index = 0;
         index < PACH_ACHIEVEMENT_TITLE_CAPACITY;
         ++index) {

        notification->title[index] = '\0';
    }
}

static int pach_notification_copy_title(
    char output[PACH_ACHIEVEMENT_TITLE_CAPACITY],
    const char *title)
{
    u32 index;

    if (output == NULL || title == NULL) {
        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    if (title[0] == '\0') {
        return PACH_NOTIFICATION_ERROR_TITLE;
    }

    for (index = 0;
         index + 1u < PACH_ACHIEVEMENT_TITLE_CAPACITY;
         ++index) {

        output[index] = title[index];

        if (title[index] == '\0') {
            return PACH_NOTIFICATION_OK;
        }
    }

    output[PACH_ACHIEVEMENT_TITLE_CAPACITY - 1u] = '\0';

    return PACH_NOTIFICATION_OK;
}

static int pach_notification_lock(
    PachNotificationQueue *queue)
{
    if (queue == NULL ||
        !queue->initialized ||
        queue->mutex_id < 0) {

        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    if (sceKernelWaitSema(
            queue->mutex_id,
            1,
            NULL) < 0) {

        return PACH_NOTIFICATION_ERROR_LOCK;
    }

    return PACH_NOTIFICATION_OK;
}

static void pach_notification_unlock(
    PachNotificationQueue *queue)
{
    if (queue == NULL ||
        !queue->initialized ||
        queue->mutex_id < 0) {

        return;
    }

    sceKernelSignalSema(
        queue->mutex_id,
        1
    );
}

static int pach_notification_enqueue_one(
    PachNotificationQueue *queue,
    const PachNotification *notification)
{
    int result;

    if (queue == NULL || notification == NULL) {
        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    result = pach_notification_lock(queue);

    if (result < 0) {
        return result;
    }

    if (queue->count >=
        PACH_NOTIFICATION_QUEUE_CAPACITY) {

        pach_notification_unlock(queue);
        return PACH_NOTIFICATION_ERROR_FULL;
    }

    queue->items[queue->tail] = *notification;

    queue->tail =
        (queue->tail + 1u) %
        PACH_NOTIFICATION_QUEUE_CAPACITY;

    ++queue->count;

    pach_notification_unlock(queue);

    return PACH_NOTIFICATION_OK;
}

void pach_notification_queue_reset(
    PachNotificationQueue *queue)
{
    u32 index;

    if (queue == NULL) {
        return;
    }

    for (index = 0;
         index < PACH_NOTIFICATION_QUEUE_CAPACITY;
         ++index) {

        pach_notification_clear(
            &queue->items[index]
        );
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->mutex_id = -1;
    queue->initialized = 0;
}

int pach_notification_queue_init(
    PachNotificationQueue *queue)
{
    SceUID mutex_id;

    if (queue == NULL) {
        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    pach_notification_queue_reset(queue);

    mutex_id = sceKernelCreateSema(
        "PachNotifyMutex",
        0,
        1,
        1,
        NULL
    );

    if (mutex_id < 0) {
        return PACH_NOTIFICATION_ERROR_CREATE;
    }

    queue->mutex_id = mutex_id;
    queue->initialized = 1;

    return PACH_NOTIFICATION_OK;
}

void pach_notification_queue_shutdown(
    PachNotificationQueue *queue)
{
    SceUID mutex_id;

    if (queue == NULL) {
        return;
    }

    mutex_id = queue->mutex_id;

    queue->initialized = 0;
    queue->mutex_id = -1;

    if (mutex_id >= 0) {
        sceKernelDeleteSema(mutex_id);
    }

    pach_notification_queue_reset(queue);
}

int pach_notification_enqueue(
    PachNotificationQueue *queue,
    u32 achievement_id,
    u16 points,
    u8 type,
    const char *title)
{
    PachNotification notification;
    int result;

    if (queue == NULL ||
        achievement_id == 0 ||
        title == NULL) {

        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    pach_notification_clear(&notification);

    notification.achievement_id = achievement_id;
    notification.points = points;
    notification.type = type;
    notification.kind = PACH_NOTIFICATION_KIND_ACHIEVEMENT;

    result = pach_notification_copy_title(
        notification.title,
        title
    );

    if (result < 0) {
        return result;
    }

    return pach_notification_enqueue_one(
        queue,
        &notification
    );
}

int pach_notification_enqueue_status(
    PachNotificationQueue *queue,
    u16 achievement_count)
{
    PachNotification notification;
    int result;

    if (queue == NULL || achievement_count == 0) {
        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    pach_notification_clear(&notification);

    notification.points = achievement_count;
    notification.kind = PACH_NOTIFICATION_KIND_STATUS;

    result = pach_notification_copy_title(
        notification.title,
        "Achievements loaded"
    );

    if (result < 0) {
        return result;
    }

    return pach_notification_enqueue_one(
        queue,
        &notification
    );
}

int pach_notification_dequeue(
    PachNotificationQueue *queue,
    PachNotification *notification)
{
    int result;

    if (queue == NULL || notification == NULL) {
        return PACH_NOTIFICATION_ERROR_ARGUMENT;
    }

    pach_notification_clear(notification);

    result = pach_notification_lock(queue);

    if (result < 0) {
        return result;
    }

    if (queue->count == 0) {
        pach_notification_unlock(queue);
        return PACH_NOTIFICATION_EMPTY;
    }

    *notification = queue->items[queue->head];

    pach_notification_clear(
        &queue->items[queue->head]
    );

    queue->head =
        (queue->head + 1u) %
        PACH_NOTIFICATION_QUEUE_CAPACITY;

    --queue->count;

    pach_notification_unlock(queue);

    return PACH_NOTIFICATION_OK;
}
