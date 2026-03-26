#include <trx/core/event_manager.h>

#include <trx/core/memory.h>
#include <trx/core/vector.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t listener_id;
    const char *event_name;
    const void *sender;
    EVENT_LISTENER listener;
    void *user_data;
} M_LISTENER;

typedef struct EVENT_MANAGER {
    VECTOR *listeners;
    int32_t listener_id;
} EVENT_MANAGER;

EVENT_MANAGER *EventManager_Create(void)
{
    EVENT_MANAGER *manager = Memory_Alloc(sizeof(EVENT_MANAGER));
    manager->listeners = Vector_Create(sizeof(M_LISTENER));
    manager->listener_id = 0;
    return manager;
}

void EventManager_Free(EVENT_MANAGER *const manager)
{
    if (manager == nullptr) {
        return;
    }
    Vector_Free(manager->listeners);
    Memory_Free(manager);
}

int32_t EventManager_Subscribe(
    EVENT_MANAGER *const manager, const char *const event_name,
    const void *const sender, const EVENT_LISTENER listener,
    void *const user_data)
{
    M_LISTENER entry = {
        .listener_id = manager->listener_id++,
        .event_name = event_name,
        .sender = sender,
        .listener = listener,
        .user_data = user_data,
    };
    Vector_Add(manager->listeners, &entry);
    return entry.listener_id;
}

void EventManager_Unsubscribe(
    EVENT_MANAGER *const manager, const int32_t listener_id)
{
    for (int32_t i = 0; i < manager->listeners->count; i++) {
        M_LISTENER entry = *(M_LISTENER *)Vector_Get(manager->listeners, i);
        if (entry.listener_id == listener_id) {
            Vector_RemoveAt(manager->listeners, i);
            return;
        }
    }
}

void EventManager_Fire(EVENT_MANAGER *const manager, const EVENT *const event)
{
    for (int32_t i = 0; i < manager->listeners->count; i++) {
        M_LISTENER entry = *(M_LISTENER *)Vector_Get(manager->listeners, i);
        if (strcmp(entry.event_name, event->name) == 0
            && entry.sender == event->sender) {
            entry.listener(event, entry.user_data);
        }
    }
}
