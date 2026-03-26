#include <trx/game/objects.h>
#include <trx/game/output.h>

typedef struct {
    RGB_888 color;
} M_PRIV;

static void M_InitialiseGeneric(const int16_t item_num, const RGB_888 color)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;
    p->color = color;
}

static void M_Control(const int16_t item_num)
{
    ITEM *const item = Item_Get(item_num);
    M_PRIV *const p = item->priv;

    if (!Item_IsTriggerActive(item)) {
        return;
    }

    Output_AddDynamicLightRGB(item->pos, 24, p->color);
}

static void M_SetupCommon(OBJECT *const obj)
{
    obj->priv_size = sizeof(M_PRIV);
    obj->control_func = M_Control;
    obj->draw_func = nullptr;
    obj->save_flags = true;
}

#define M_DEFINE_LIGHT(name, r, g, b)                                          \
    static void M_Initialise##name(const int16_t num)                          \
    {                                                                          \
        M_InitialiseGeneric(num, (RGB_888) { r, g, b });                       \
    }                                                                          \
    static void M_Setup##name(OBJECT *const obj)                               \
    {                                                                          \
        M_SetupCommon(obj);                                                    \
        obj->initialise_func = M_Initialise##name;                             \
    }                                                                          \
    REGISTER_OBJECT(O_##name##_LIGHT, M_Setup##name)

M_DEFINE_LIGHT(RED, 255, 0, 0)
M_DEFINE_LIGHT(GREEN, 0, 255, 0)
M_DEFINE_LIGHT(BLUE, 0, 0, 255)
M_DEFINE_LIGHT(AMBER, 255, 192, 0)
M_DEFINE_LIGHT(WHITE, 224, 224, 255)

#undef M_DEFINE_LIGHT
