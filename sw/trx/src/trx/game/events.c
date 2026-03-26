#include <trx/game/events.h>

#include <trx/config/common.h>
#include <trx/debug.h>

static EVENT_MANAGER *m_GameEventManager = nullptr;

void GameEvent_Init(void)
{
    m_GameEventManager = EventManager_Create();
}

void GameEvent_Shutdown(void)
{
    EventManager_Free(m_GameEventManager);
    m_GameEventManager = nullptr;
}

int32_t GameEvent_Subscribe(
    const char *const event_name, const void *const sender,
    const GAME_EVENT_LISTENER listener, void *const user_data)
{
    ASSERT(m_GameEventManager != nullptr);
    return EventManager_Subscribe(
        m_GameEventManager, event_name, sender, listener, user_data);
}

void GameEvent_Unsubscribe(const int32_t listener_id)
{
    if (m_GameEventManager != nullptr) {
        EventManager_Unsubscribe(m_GameEventManager, listener_id);
    }
}

void GameEvent_Fire(const EVENT event)
{
    if (m_GameEventManager != nullptr) {
        EventManager_Fire(m_GameEventManager, &event);
    }
}
