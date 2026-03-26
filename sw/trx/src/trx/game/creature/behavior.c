#include <trx/config.h>
#include <trx/core/vector.h>
#include <trx/game/creature.h>
#include <trx/game/objects/vars.h>
#include <trx/game/stats.h>

#define M_ALLY_FRIENDLY_FIRE_THRESHOLD 10

static bool m_AlliesHostile = false;
static VECTOR *m_AllyObjects = nullptr;
static VECTOR *m_AllyTargetingObjects = nullptr;

__attribute__((constructor)) static void M_Init(void)
{
    m_AllyObjects = Vector_Create(sizeof(OBJECT_ID));
    m_AllyTargetingObjects = Vector_Create(sizeof(OBJECT_ID));
}

__attribute__((destructor)) static void M_Shutdown(void)
{
#define L_DELETE_VECTOR(vec)                                                   \
    if (vec != nullptr) {                                                      \
        Vector_Free(vec);                                                      \
        vec = nullptr;                                                         \
    }

    L_DELETE_VECTOR(m_AllyObjects);
    L_DELETE_VECTOR(m_AllyTargetingObjects);

#undef L_DELETE_VECTOR
}

void Creature_Reset(void)
{
    Creature_SetAlliesHostile(false);
    Vector_Clear(m_AllyObjects);
    Vector_Clear(m_AllyTargetingObjects);
}

bool Creature_AreAlliesHostile(void)
{
    return m_AlliesHostile;
}

void Creature_SetAlliesHostile(bool enable)
{
    m_AlliesHostile = enable;
    if (enable) {
        Stats_MarkAlliesHostile();
    }
}

void Creature_Hurt(ITEM *const item, const int32_t damage)
{
    if (damage <= 0) {
        return;
    }

    CREATURE *const creature = item->creature_data;
    if (creature != nullptr) {
        creature->hurt_by_lara = true;
    }

    switch (g_Config.gameplay.ally_hostility_policy) {
    case ALLY_HOSTILITY_POLICY_INDIVIDUAL:
        Stats_MarkAlliesHostile();
        break;

    case ALLY_HOSTILITY_POLICY_SHARED:
        if (!m_AlliesHostile && Creature_IsAlly(item)) {
            if (creature != nullptr) {
                creature->damage_from_lara += damage;
            }
            if (item->hit_points <= 0
                || (creature != nullptr
                    && (creature->damage_from_lara
                            > M_ALLY_FRIENDLY_FIRE_THRESHOLD
                        || creature->mood == MOOD_BORED))) {
                m_AlliesHostile = true;
                Stats_MarkAlliesHostile();
            }
        }
        break;
    }
}

bool Creature_IsHostile(const ITEM *const item)
{
    if (item->object_id != O_SKIDOO_ARMED
        && !Object_IsType(item->object_id, g_CreatureObjects)) {
        return false;
    }

    if (!Creature_IsAlly(item)) {
        return true;
    }

    switch (g_Config.gameplay.ally_hostility_policy) {
    case ALLY_HOSTILITY_POLICY_INDIVIDUAL:
        const CREATURE *const creature = item->creature_data;
        return creature != nullptr && creature->hurt_by_lara;
    case ALLY_HOSTILITY_POLICY_SHARED:
        return m_AlliesHostile;
    }
    return false;
}

bool Creature_IsAlly(const ITEM *const item)
{
    return Vector_Contains(m_AllyObjects, (void *)&item->object_id);
}

bool Creature_IsAllyTargetingEnemy(const ITEM *const item)
{
    return Vector_Contains(m_AllyTargetingObjects, (void *)&item->object_id);
}

void Creature_AddAlly(const OBJECT_ID obj_id)
{
    Vector_Add(m_AllyObjects, (void *)&obj_id);
}

void Creature_AddAllyTargetingEnemy(const OBJECT_ID obj_id)
{
    Vector_Add(m_AllyTargetingObjects, (void *)&obj_id);
}
