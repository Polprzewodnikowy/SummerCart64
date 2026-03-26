#pragma once

#include <trx/core/event_manager.h>

typedef void (*EVENT_LISTENER)(const EVENT *, void *user_data);

void UI_InitEvents(void);
void UI_ShutdownEvents(void);

int32_t UI_Subscribe(
    const char *event_name, const void *sender, EVENT_LISTENER listener,
    void *user_data);

void UI_Unsubscribe(int32_t listener_id);

void UI_FireEvent(EVENT event);
