#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    const void *sender;
    const void *data;
} EVENT;

typedef void (*EVENT_LISTENER)(const EVENT *, void *user_data);

typedef struct EVENT_MANAGER EVENT_MANAGER;

EVENT_MANAGER *EventManager_Create(void);
void EventManager_Free(EVENT_MANAGER *manager);

int32_t EventManager_Subscribe(
    EVENT_MANAGER *manager, const char *event_name, const void *sender,
    EVENT_LISTENER listener, void *user_data);

void EventManager_Unsubscribe(EVENT_MANAGER *manager, int32_t listener_id);

void EventManager_Fire(EVENT_MANAGER *manager, const EVENT *event);
