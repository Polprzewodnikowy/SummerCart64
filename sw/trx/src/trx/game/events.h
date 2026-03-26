#pragma once

#include <trx/core/event_manager.h>

// Game-level event names
#define GAME_EVENT_SCREENSHOT "screenshot"
#define GAME_EVENT_COMMAND "console_command"

typedef void (*GAME_EVENT_LISTENER)(const EVENT *event, void *user_data);

void GameEvent_Init(void);
void GameEvent_Shutdown(void);

int32_t GameEvent_Subscribe(
    const char *event_name, const void *sender, GAME_EVENT_LISTENER listener,
    void *user_data);
void GameEvent_Unsubscribe(int32_t listener_id);

void GameEvent_Fire(const EVENT event);
