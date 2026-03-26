#include <trx/game/ui/events.h>

#include <trx/config/common.h>
#include <trx/debug.h>
#include <trx/game/ui/common.h>

static EVENT_MANAGER *m_EventManager = nullptr;

void UI_InitEvents(void)
{
    m_EventManager = EventManager_Create();
}

void UI_ShutdownEvents(void)
{
    EventManager_Free(m_EventManager);
    m_EventManager = nullptr;
}

int32_t UI_Subscribe(
    const char *const event_name, const void *const sender,
    const EVENT_LISTENER listener, void *const user_data)
{
    ASSERT(m_EventManager != nullptr);
    return EventManager_Subscribe(
        m_EventManager, event_name, sender, listener, user_data);
}

void UI_Unsubscribe(const int32_t listener_id)
{
    if (m_EventManager != nullptr) {
        EventManager_Unsubscribe(m_EventManager, listener_id);
    }
}

void UI_FireEvent(const EVENT event)
{
    if (m_EventManager != nullptr) {
        EventManager_Fire(m_EventManager, &event);
    }
}
