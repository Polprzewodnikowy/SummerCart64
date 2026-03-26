#include <trx/game/option/passport.h>

#include <trx/config.h>
#include <trx/debug.h>
#include <trx/game/game.h>
#include <trx/game/game_flow.h>
#include <trx/game/game_strings/entries.h>
#include <trx/game/input.h>
#include <trx/game/inventory.h>
#include <trx/game/inventory_ring.h>
#include <trx/game/overlay.h>
#include <trx/game/savegame.h>
#include <trx/game/shell/common.h>
#include <trx/game/sound.h>
#include <trx/game/ui.h>
#include <trx/version.h>

#define M_IMMEDIATE (g_TRVersion >= 2)

typedef enum {
    M_ROLE_PLAY_ANY_LEVEL_SELECT_LEVEL,
    M_ROLE_PLAY_ANY_LEVEL_SELECT_MODE,
    M_ROLE_PLAY_PREV_LEVEL_SELECT_SLOT,
    M_ROLE_PLAY_PREV_LEVEL_SELECT_LEVEL,
    M_ROLE_SWITCH_MOD,
    M_ROLE_STORY_SO_FAR,
    M_ROLE_STORY_SO_FAR_CONFIRM,
    M_ROLE_NEW_GAME,
    M_ROLE_LOAD_GAME,
    M_ROLE_SAVE_GAME,
    M_ROLE_RESTART_LEVEL,
    M_ROLE_EXIT_TO_TITLE,
    M_ROLE_EXIT_GAME,
} M_PAGE_ROLE;

typedef struct {
    GAME_STRING_ID title;
    bool (*func)(INVENTORY_ITEM *inv_item);
    bool flat;
} M_PAGE_HANDLER;

typedef enum {
    PAGE_UNDETERMINED = -1,
    PAGE_1 = 0,
    PAGE_2 = 1,
    PAGE_3 = 2,
    PAGE_COUNT = 3,
} M_PAGE_NUMBER;

typedef enum {
    M_MODE_BROWSE,
    M_MODE_PICK_OPTION,
} M_PAGE_MODE;

typedef struct {
    M_PAGE_ROLE role;
    int32_t selection;

    struct {
        UI_NEW_GAME_STATE *new_game;
        UI_SELECT_LEVEL_DIALOG_STATE *select_level;
        UI_PLAY_ANY_LEVEL_DIALOG_STATE *play_any_level;
        UI_SAVE_SLOT_DIALOG_STATE *save_slot;
        UI_SWITCH_MOD_DIALOG_STATE *switch_mod;
    } ui;
} M_NAV_FRAME;

typedef struct {
    bool available;
    M_PAGE_ROLE role;

    struct { // Hierarchical navigation
        int32_t depth;
        M_NAV_FRAME stack[4];
        M_NAV_FRAME *current;
    } nav;
} M_PAGE;

PASSPORT g_Passport = {
    .select_level = -1,
};

static struct {
    M_PAGE_MODE mode;
    M_PAGE pages[PAGE_COUNT];
    M_PAGE_NUMBER current_page;
    M_PAGE_NUMBER active_page;
    GAME_STRING_ID error_msg;
} m_Priv = {
    .active_page = PAGE_UNDETERMINED,
};

static void M_ResetNavStack(M_PAGE *const page)
{
    ASSERT(page != nullptr);
    page->nav.depth = 0;
    page->nav.current = &page->nav.stack[page->nav.depth];
    page->nav.current->selection = -1;
    page->nav.current->role = page->role;
}

static void M_InitText(void)
{
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCL, false);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCR, false);
    Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
}

static void M_FreeDialogs(M_NAV_FRAME *const frame)
{
    if (frame->ui.select_level != nullptr) {
        UI_SelectLevelDialog_Free(frame->ui.select_level);
        frame->ui.select_level = nullptr;
    }
    if (frame->ui.play_any_level != nullptr) {
        UI_PlayAnyLevelDialog_Free(frame->ui.play_any_level);
        frame->ui.play_any_level = nullptr;
    }
    if (frame->ui.save_slot != nullptr) {
        UI_SaveSlotDialog_Free(frame->ui.save_slot);
        frame->ui.save_slot = nullptr;
    }
    if (frame->ui.switch_mod != nullptr) {
        UI_SwitchModDialog_Free(frame->ui.switch_mod);
        frame->ui.switch_mod = nullptr;
    }
    if (frame->ui.new_game != nullptr) {
        UI_NewGame_Free(frame->ui.new_game);
        frame->ui.new_game = nullptr;
    }
}

static void M_FreeAllDialogs(void)
{
    for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
        for (int32_t j = 0; j <= m_Priv.pages[i].nav.depth; j++) {
            M_FreeDialogs(&m_Priv.pages[i].nav.stack[j]);
        }
    }
}

static void M_RemoveAllText(void)
{
    m_Priv.error_msg = nullptr;
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCL, false);
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCR, false);
    Overlay_SetBottomText((OVERLAY_TEXT) { 0 });
}

static M_PAGE *M_TryGetActivePage(void)
{
    if (m_Priv.active_page < 0 || m_Priv.active_page >= PAGE_COUNT) {
        return nullptr;
    }
    return &m_Priv.pages[m_Priv.active_page];
}

static M_PAGE *M_GetActivePage(void)
{
    M_PAGE *const page = M_TryGetActivePage();
    ASSERT(page != nullptr);
    return page;
}

static bool M_IsArrowVisible(int32_t direction)
{
    if (m_Priv.mode == M_MODE_PICK_OPTION && !M_IMMEDIATE) {
        return false;
    }
    const M_PAGE *const page = M_TryGetActivePage();
    if (page == nullptr || page->nav.depth > 0) {
        return false;
    }
    for (M_PAGE_NUMBER i = m_Priv.active_page + direction;
         i >= PAGE_1 && i < PAGE_COUNT; i += direction) {
        if (m_Priv.pages[i].available) {
            return true;
        }
    }
    return false;
}

static void M_SyncArrowsVisibility(void)
{
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCL, M_IsArrowVisible(-1));
    Overlay_ShowArrow(UI_OVERLAY_ARROW_BCR, M_IsArrowVisible(1));
}

static void M_ChangePageTextContent(const char *const content)
{
    InvRing_RemoveAllText();
    Overlay_SetBottomText((OVERLAY_TEXT) {
        .kind = UI_OVERLAY_TEXT_LITERAL,
        .literal = content,
        .fmt_gs_key = GS_ID("general/inventory_ring/object_name_fmt"),
    });
}

static M_PAGE_NUMBER M_GetCurrentPage(const INVENTORY_ITEM *const inv_item)
{
    const int32_t frame = inv_item->goal_frame - inv_item->open_frame;
    return frame % 5 == 0 ? frame / 5 : PAGE_UNDETERMINED;
}

static bool M_IsFlipping(const INVENTORY_ITEM *const inv_item)
{
    return M_GetCurrentPage(inv_item) == PAGE_UNDETERMINED;
}

static void M_FlipLeft(INVENTORY_ITEM *const inv_item)
{
    M_RemoveAllText();
    inv_item->anim_direction = -1;
    inv_item->goal_frame = inv_item->open_frame + 5 * m_Priv.active_page;
    Sound_Effect(SFX_MENU_PASSPORT, nullptr, SPM_ALWAYS);
}

static void M_FlipRight(INVENTORY_ITEM *const inv_item)
{
    M_RemoveAllText();
    inv_item->anim_direction = 1;
    inv_item->goal_frame = inv_item->open_frame + 5 * m_Priv.active_page;
    Sound_Effect(SFX_MENU_PASSPORT, nullptr, SPM_ALWAYS);
}

static void M_Close(INVENTORY_ITEM *const inv_item)
{
    m_Priv.active_page = PAGE_UNDETERMINED;
    M_RemoveAllText();
    M_FreeAllDialogs();
    if (m_Priv.current_page == PAGE_3) {
        inv_item->anim_direction = 1;
        inv_item->goal_frame = inv_item->frames_total - 1;
    } else {
        inv_item->anim_direction = -1;
        inv_item->goal_frame = 0;
    }
}

static void M_SoftClose(INVENTORY_ITEM *const inv_item)
{
    if (g_Inv_Mode == INV_DEATH_MODE) {
        if (!M_IMMEDIATE && m_Priv.mode != M_MODE_BROWSE) {
            m_Priv.mode = M_MODE_BROWSE;
        }
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
        return;
    }
    if (m_Priv.mode == M_MODE_BROWSE || M_IMMEDIATE
        || (g_Inv_Mode != INV_GAME_MODE && g_Inv_Mode != INV_TITLE_MODE)) {
        M_Close(inv_item);
    } else {
        m_Priv.mode = M_MODE_BROWSE;
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    }
}

static void M_NavigateInto(const M_PAGE_ROLE role, const int32_t selection)
{
    M_PAGE *const page = M_GetActivePage();
    if (page->nav.depth + 1
        < (int32_t)(sizeof page->nav.stack / sizeof page->nav.stack[0])) {
        page->nav.current->selection = selection;
        page->nav.depth++;
        page->nav.current = &page->nav.stack[page->nav.depth];
        page->nav.current->role = role;
        page->nav.current->selection = -1;
    }
    g_Input = (INPUT_STATE) {};
    g_InputDB = (INPUT_STATE) {};
}

static void M_NavigateOut(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_TryGetActivePage();
    if (page == nullptr) {
        return;
    }
    m_Priv.error_msg = nullptr;
    M_FreeDialogs(page->nav.current);
    if (page->nav.depth > 0) {
        page->nav.depth--;
        page->nav.current = &page->nav.stack[page->nav.depth];
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    } else {
        M_SoftClose(inv_item);
    }
}

static void M_Confirm(const PASSPORT_ACTION role, const int32_t argument)
{
    g_Passport.select_action = role;
    g_Passport.select_level = argument;
}

static void M_ConfirmSaveSlot(
    const PASSPORT_ACTION role, const SAVEGAME_SLOT_REF slot)
{
    g_Passport.select_action = role;
    g_Passport.select_save_slot = slot;
}

static void M_SetPage(
    const M_PAGE_NUMBER page, const M_PAGE_ROLE role, const bool available)
{
    m_Priv.pages[page].role = role;
    m_Priv.pages[page].available = available;
}

static void M_DeterminePages(void)
{
    const bool can_restart = Savegame_RestartAvailable(Savegame_GetBoundSlot());
    const bool saving_enabled =
        Savegame_GetSlotCount(SAVEGAME_SLOT_POOL_NORMAL) > 0
        && !g_Config.flow.load_save_disabled;
    const bool has_saves = Savegame_GetTotalCount() > 0 && saving_enabled;

    for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
        m_Priv.pages[i].available = false;
    }

    switch (g_Inv_Mode) {
    case INV_TITLE_MODE:
        m_Priv.mode = M_IMMEDIATE ? M_MODE_PICK_OPTION : M_MODE_BROWSE;
        M_SetPage(PAGE_1, M_ROLE_LOAD_GAME, has_saves);
        M_SetPage(PAGE_2, M_ROLE_NEW_GAME, true);
        M_SetPage(PAGE_3, M_ROLE_EXIT_GAME, true);
        break;

    case INV_GAME_MODE:
        m_Priv.mode = M_IMMEDIATE ? M_MODE_PICK_OPTION : M_MODE_BROWSE;
        if (!saving_enabled) {
            M_SetPage(PAGE_2, M_ROLE_RESTART_LEVEL, can_restart);
        } else {
            M_SetPage(PAGE_1, M_ROLE_LOAD_GAME, has_saves);
            M_SetPage(PAGE_2, M_ROLE_SAVE_GAME, true);
        }
        M_SetPage(PAGE_3, M_ROLE_EXIT_TO_TITLE, true);
        break;

    case INV_LOAD_MODE:
        m_Priv.mode = M_MODE_PICK_OPTION;
        if (!saving_enabled) {
            M_SetPage(PAGE_2, M_ROLE_RESTART_LEVEL, can_restart);
        } else if (has_saves) {
            M_SetPage(PAGE_1, M_ROLE_LOAD_GAME, true);
        } else {
            M_SetPage(PAGE_2, M_ROLE_SAVE_GAME, true);
        }
        break;

    case INV_SAVE_MODE:
    case INV_SAVE_CRYSTAL_MODE:
        m_Priv.mode = M_MODE_PICK_OPTION;
        if (!saving_enabled) {
            M_SetPage(PAGE_2, M_ROLE_RESTART_LEVEL, can_restart);
        } else {
            M_SetPage(PAGE_2, M_ROLE_SAVE_GAME, true);
        }
        break;

    case INV_DEATH_MODE:
        m_Priv.mode = M_IMMEDIATE ? M_MODE_PICK_OPTION : M_MODE_BROWSE;
        M_SetPage(PAGE_1, M_ROLE_LOAD_GAME, has_saves);
        M_SetPage(PAGE_2, M_ROLE_RESTART_LEVEL, can_restart);
        M_SetPage(PAGE_3, M_ROLE_EXIT_TO_TITLE, true);
        break;

    case INV_KEYS_MODE:
    case INV_GLOBE_SELECT_MODE:
        ASSERT_FAIL();
    }

    // Disable saves in gym and save crystals mode.
    // Offer New Game or Restart instead.
    for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
        if (m_Priv.pages[i].role != M_ROLE_SAVE_GAME) {
            continue;
        }
        if (Game_IsInGym()) {
            m_Priv.pages[i].role = M_ROLE_NEW_GAME;
        } else if (
            g_Config.gameplay.enable_save_crystals
            && g_Inv_Mode != INV_SAVE_CRYSTAL_MODE) {
            if (can_restart) {
                m_Priv.pages[i].role = M_ROLE_RESTART_LEVEL;
            } else {
                m_Priv.pages[i].available = false;
            }
        }
    }

    // If play any level is enabled, replace New Game with Play Any Level.
    if (g_Config.flow.play_any_level) {
        for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
            if (m_Priv.pages[i].role == M_ROLE_NEW_GAME) {
                m_Priv.pages[i].role = M_ROLE_PLAY_ANY_LEVEL_SELECT_LEVEL;
            }
        }
    }

    // Select first available page
    m_Priv.active_page = PAGE_UNDETERMINED;
    for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
        if (m_Priv.pages[i].available) {
            m_Priv.active_page = i;
            break;
        }
    }

    // Guard: if no pages are available, force-add exit game or exit to title
    if (m_Priv.active_page == PAGE_UNDETERMINED) {
        M_SetPage(
            PAGE_3,
            g_Inv_Mode == INV_TITLE_MODE ? M_ROLE_EXIT_GAME
                                         : M_ROLE_EXIT_TO_TITLE,
            true);
        m_Priv.active_page = PAGE_3;
    }

    // reset hierarchical nav stack now that top-level pages are set
    ASSERT(m_Priv.active_page != PAGE_UNDETERMINED);
    for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
        M_ResetNavStack(&m_Priv.pages[i]);
    }

    for (M_PAGE_NUMBER i = PAGE_1; i < PAGE_COUNT; i++) {
        LOG_DEBUG(
            "page %d: role=%d available=%d", i, m_Priv.pages[i].role,
            m_Priv.pages[i].available);
    }
}

static bool M_ChooseSaveSlot(
    INVENTORY_ITEM *const inv_item, const UI_SAVE_SLOT_DIALOG_TYPE dialog_type,
    SAVEGAME_SLOT_REF *const selected_slot)
{
    *selected_slot = Savegame_InvalidSlot();
    M_PAGE *const page = M_GetActivePage();
    M_NAV_FRAME *const frame = page->nav.current;
    if (frame->ui.save_slot == nullptr) {
        const int32_t selection = page->nav.stack[page->nav.depth].selection;
        SAVEGAME_SLOT_REF initial_slot = selection != -1
            ? Savegame_SlotFromParam(selection)
            : Savegame_InvalidSlot();
        if (!Savegame_IsValidSlotRef(initial_slot)) {
            initial_slot = Savegame_GetMostRecentlyUsedSlot();
        }
        if (!Savegame_IsValidSlotRef(initial_slot)) {
            initial_slot = Savegame_GetMostRecentlyCreatedSlot();
        }
        if (!Savegame_IsValidSlotRef(initial_slot)) {
            initial_slot = Savegame_NormalSlot(0);
        }
        page->nav.current->ui.save_slot =
            UI_SaveSlotDialog_Init(dialog_type, initial_slot);
    }
    const UI_SAVE_SLOT_DIALOG_CHOICE choice =
        UI_SaveSlotDialog_Control(frame->ui.save_slot);
    switch (choice.action) {
    case UI_SAVE_SLOT_DIALOG_NO_CHOICE:
        if (M_IMMEDIATE) {
            // Make sure it's not possible to confirm empty slots
            g_Input.menu_confirm = false;
            g_InputDB.menu_confirm = false;
        } else {
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }
        return false;
    case UI_SAVE_SLOT_DIALOG_CANCEL:
        M_NavigateOut(inv_item);
        return true;
    case UI_SAVE_SLOT_DIALOG_CONFIRM:
        *selected_slot = choice.slot;
        return true;
    }
    return false;
}

static bool M_CheckConfirm(const PASSPORT_ACTION action)
{
    if (g_InputDB.menu_confirm) {
        M_Confirm(action, -1);
        return true;
    }
    return false;
}

static bool M_HandleLoadGame(INVENTORY_ITEM *const inv_item)
{
    SAVEGAME_SLOT_REF selected_slot = Savegame_InvalidSlot();
    const bool result = M_ChooseSaveSlot(
        inv_item, UI_SAVE_SLOT_DIALOG_LOAD_GAME, &selected_slot);
    if (Savegame_IsValidSlotRef(selected_slot)) {
        M_ConfirmSaveSlot(PASSPORT_ACTION_LOAD_GAME, selected_slot);
    }
    return result;
}

static bool M_HandleSaveGame(INVENTORY_ITEM *const inv_item)
{
    SAVEGAME_SLOT_REF selected_slot = Savegame_InvalidSlot();
    const bool result = M_ChooseSaveSlot(
        inv_item, UI_SAVE_SLOT_DIALOG_SAVE_GAME, &selected_slot);
    if (Savegame_IsValidSlotRef(selected_slot)) {
        M_ConfirmSaveSlot(PASSPORT_ACTION_SAVE_GAME, selected_slot);
    }
    return result;
}

static bool M_HandleNewGame(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_GetActivePage();
    M_NAV_FRAME *const frame = page->nav.current;

    // If no options – start the game already
    if (!g_Config.gameplay.enable_game_modes
        && !g_Config.profile.new_game_plus_unlock
        && !g_Config.gameplay.enable_play_previous_levels
        && !UI_NewGame_HasModChoices()) {
        // But only if in title mode
        if (g_InputDB.menu_confirm
            || (!M_IMMEDIATE && g_Inv_Mode == INV_TITLE_MODE)) {
            M_Confirm(PASSPORT_ACTION_NEW_GAME, GF_GetFirstLevel()->num);
            g_InputDB.menu_confirm = true;
            M_Close(inv_item);
        }
        return false;
    }

    if (frame->ui.new_game == nullptr) {
        frame->ui.new_game = UI_NewGame_Init(true);
    }
    const int32_t choice = UI_NewGame_Control(frame->ui.new_game);
    if (choice == UI_REQUESTER_NO_CHOICE) {
        if (!M_IMMEDIATE) {
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }
        return false;
    } else {
        switch (choice) {
        case UI_REQUESTER_CANCEL:
            M_NavigateOut(inv_item);
            return true;

        case UI_NEW_GAME_CHOICE_NG:
            // Handle the scenario where enable_game_modes is off, and
            // enable_play_previous_levels is on. In this scenario the dialog
            // adds a "New Game" row just to let the player start the game. It
            // shouldn't touch the NG+ flag.
            if (g_Config.gameplay.enable_game_modes) {
                Game_SetBonusFlag(GBF_NONE);
            }
            M_Confirm(PASSPORT_ACTION_NEW_GAME, GF_GetFirstLevel()->num);
            return true;
        case UI_NEW_GAME_CHOICE_NGPLUS:
            Game_SetBonusFlag(GBF_NGPLUS);
            M_Confirm(PASSPORT_ACTION_NEW_GAME, GF_GetFirstLevel()->num);
            return true;
        case UI_NEW_GAME_CHOICE_JP_NG:
            Game_SetBonusFlag(GBF_JAPANESE);
            M_Confirm(PASSPORT_ACTION_NEW_GAME, GF_GetFirstLevel()->num);
            return true;
        case UI_NEW_GAME_CHOICE_JP_NGPLUS:
            Game_SetBonusFlag(GBF_JAPANESE | GBF_NGPLUS);
            M_Confirm(PASSPORT_ACTION_NEW_GAME, GF_GetFirstLevel()->num);
            return true;
        case UI_NEW_GAME_CHOICE_SWITCH_MOD:
            M_NavigateInto(M_ROLE_SWITCH_MOD, -1);
            return true;
        case UI_NEW_GAME_CHOICE_PLAY_PREV_LEVELS:
            M_NavigateInto(M_ROLE_PLAY_PREV_LEVEL_SELECT_SLOT, -1);
            return true;
        case UI_NEW_GAME_CHOICE_STORY_SO_FAR:
            M_NavigateInto(M_ROLE_STORY_SO_FAR, -1);
            return true;
        }
    }
    return false;
}

static bool M_HandlePlayAnyLevel(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_GetActivePage();
    M_NAV_FRAME *const frame = page->nav.current;
    if (frame->ui.play_any_level == nullptr) {
        frame->ui.play_any_level = UI_PlayAnyLevelDialog_Init();
    }
    const int32_t choice =
        UI_PlayAnyLevelDialog_Control(frame->ui.play_any_level);
    if (choice == UI_REQUESTER_NO_CHOICE) {
        if (!M_IMMEDIATE) {
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }
        return false;
    } else if (choice == UI_REQUESTER_CANCEL) {
        M_NavigateOut(inv_item);
        return true;
    } else if (
        g_Config.gameplay.enable_game_modes
        || g_Config.profile.new_game_plus_unlock) {
        M_NavigateInto(M_ROLE_PLAY_ANY_LEVEL_SELECT_MODE, choice);
        return true;
    } else {
        Savegame_UnbindSlot();
        M_Confirm(PASSPORT_ACTION_SELECT_LEVEL, choice);
        return true;
    }
}

static bool M_HandlePlayAnyLevelSelectMode(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_GetActivePage();
    M_NAV_FRAME *const frame = page->nav.current;
    ASSERT(m_Priv.mode == M_MODE_PICK_OPTION);
    if (frame->ui.new_game == nullptr) {
        frame->ui.new_game = UI_NewGame_Init(false);
    }
    const int32_t choice = UI_NewGame_Control(frame->ui.new_game);
    if (choice == UI_REQUESTER_NO_CHOICE) {
        return false;
    } else {
        const int32_t level_num =
            page->nav.stack[page->nav.depth - 1].selection;
        switch (choice) {
        case UI_REQUESTER_CANCEL:
            M_NavigateOut(inv_item);
            return true;
        case UI_NEW_GAME_CHOICE_NG:
            Game_SetBonusFlag(GBF_NONE);
            Savegame_UnbindSlot();
            M_Confirm(PASSPORT_ACTION_SELECT_LEVEL, level_num);
            return true;
        case UI_NEW_GAME_CHOICE_NGPLUS:
            Game_SetBonusFlag(GBF_NGPLUS);
            Savegame_UnbindSlot();
            M_Confirm(PASSPORT_ACTION_SELECT_LEVEL, level_num);
            return true;
        case UI_NEW_GAME_CHOICE_JP_NG:
            Game_SetBonusFlag(GBF_JAPANESE);
            Savegame_UnbindSlot();
            M_Confirm(PASSPORT_ACTION_SELECT_LEVEL, level_num);
            return true;
        case UI_NEW_GAME_CHOICE_JP_NGPLUS:
            Game_SetBonusFlag(GBF_JAPANESE | GBF_NGPLUS);
            Savegame_UnbindSlot();
            M_Confirm(PASSPORT_ACTION_SELECT_LEVEL, level_num);
            return true;
        default:
            ASSERT_FAIL();
        }
    }
    return false;
}

static bool M_HandleSwitchMod(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_GetActivePage();
    M_NAV_FRAME *const frame = page->nav.current;
    if (frame->ui.switch_mod == nullptr) {
        frame->ui.switch_mod = UI_SwitchModDialog_Init();
    }

    const int32_t choice = UI_SwitchModDialog_Control(frame->ui.switch_mod);
    if (choice == UI_REQUESTER_NO_CHOICE) {
        return false;
    }
    if (choice == UI_REQUESTER_CANCEL) {
        M_NavigateOut(inv_item);
        return true;
    }

    const char *const mod_name =
        UI_SwitchModDialog_GetSelectedMod(frame->ui.switch_mod, choice);
    Shell_RequestModSwitch(mod_name);
    M_Confirm(PASSPORT_ACTION_SWITCH_MOD, -1);
    g_InputDB.menu_confirm = true;
    M_Close(inv_item);
    return true;
}

static bool M_HandlePlayPrevLevelSelectSlot(INVENTORY_ITEM *const inv_item)
{
    SAVEGAME_SLOT_REF selected_slot = Savegame_InvalidSlot();
    const bool result =
        M_ChooseSaveSlot(inv_item, UI_SAVE_SLOT_DIALOG_GENERIC, &selected_slot);
    if (Savegame_IsValidSlotRef(selected_slot)) {
        M_NavigateInto(
            M_ROLE_PLAY_PREV_LEVEL_SELECT_LEVEL,
            Savegame_SlotToParam(selected_slot));
    }
    return result;
}

static bool M_HandlePlayPrevLevelSelectLevel(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_GetActivePage();
    const SAVEGAME_SLOT_REF slot =
        Savegame_SlotFromParam(page->nav.stack[page->nav.depth - 1].selection);
    if (!Savegame_IsValidSlotRef(slot)) {
        M_NavigateOut(inv_item);
        return true;
    }
    const SAVEGAME_INFO *const info = Savegame_GetSavegameInfo(slot);
    if (info == nullptr) {
        M_NavigateOut(inv_item);
        return true;
    }
    if (!info->features.select_level) {
        m_Priv.error_msg = GS_ID("general/passport/save_slot_unsupported");
        if (g_InputDB.menu_back || g_InputDB.menu_confirm) {
            M_NavigateOut(inv_item);
            return true;
        }
        return false;
    }
    M_NAV_FRAME *const frame = page->nav.current;
    if (frame->ui.select_level == nullptr) {
        frame->ui.select_level = UI_SelectLevelDialog_Init(slot);
    }
    const int32_t choice = UI_SelectLevelDialog_Control(frame->ui.select_level);
    if (choice == UI_REQUESTER_NO_CHOICE) {
        if (!M_IMMEDIATE) {
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
        }
        return false;
    } else if (choice == UI_REQUESTER_CANCEL) {
        M_NavigateOut(inv_item);
        return true;
    } else {
        Savegame_BindSlot(slot);
        M_Confirm(PASSPORT_ACTION_SELECT_LEVEL, choice);
        return true;
    }
    return false;
}

static bool M_HandleStorySoFar(INVENTORY_ITEM *const inv_item)
{
    SAVEGAME_SLOT_REF selected_slot = Savegame_InvalidSlot();
    const bool result =
        M_ChooseSaveSlot(inv_item, UI_SAVE_SLOT_DIALOG_GENERIC, &selected_slot);
    if (Savegame_IsValidSlotRef(selected_slot)) {
        M_NavigateInto(
            M_ROLE_STORY_SO_FAR_CONFIRM, Savegame_SlotToParam(selected_slot));
    }
    return result;
}

static bool M_HandleStorySoFarConfirm(INVENTORY_ITEM *const inv_item)
{
    M_PAGE *const page = M_GetActivePage();
    const SAVEGAME_SLOT_REF slot =
        Savegame_SlotFromParam(page->nav.stack[page->nav.depth - 1].selection);
    if (GF_HasAvailableStory(slot)) {
        M_ConfirmSaveSlot(PASSPORT_ACTION_STORY_SO_FAR, slot);
        g_InputDB.menu_confirm = true;
        M_Close(inv_item);
        return true;
    } else if (g_InputDB.menu_back || g_InputDB.menu_confirm) {
        M_NavigateOut(inv_item);
        return true;
    } else {
        m_Priv.error_msg = GS_ID("general/passport/save_slot_unsupported");
        return false;
    }
    return false;
}

static bool M_HandleRestartLevel(INVENTORY_ITEM *const inv_item)
{
    return M_CheckConfirm(PASSPORT_ACTION_RESTART);
}

static bool M_HandleExitGame(INVENTORY_ITEM *const inv_item)
{
    return M_CheckConfirm(PASSPORT_ACTION_EXIT_GAME);
}

static bool M_HandleExitToTitle(INVENTORY_ITEM *const inv_item)
{
    return M_CheckConfirm(PASSPORT_ACTION_EXIT_TO_TITLE);
}

static bool M_ShowPage(INVENTORY_ITEM *const inv_item)
{
    static M_PAGE_HANDLER m_PageHandlers[] = {
        [M_ROLE_LOAD_GAME] = {
            .title = GS_ID("general/passport/load_game"),
            .func = M_HandleLoadGame,
            .flat = false,
        },
        [M_ROLE_SAVE_GAME] = {
            .title = GS_ID("general/passport/save_game"),
            .func = M_HandleSaveGame,
            .flat = false,
        },
        [M_ROLE_NEW_GAME] = {
            .title = GS_ID("general/passport/new_game"),
            .func = M_HandleNewGame,
            .flat = false,
        },
        [M_ROLE_PLAY_ANY_LEVEL_SELECT_LEVEL] = {
            .title = GS_ID("general/passport/select_level"),
            .func = M_HandlePlayAnyLevel,
            .flat = false,
        },
        [M_ROLE_PLAY_ANY_LEVEL_SELECT_MODE] = {
            .title = GS_ID("general/passport/select_level"),
            .func = M_HandlePlayAnyLevelSelectMode,
            .flat = false,
        },
        [M_ROLE_PLAY_PREV_LEVEL_SELECT_SLOT] = {
            .title = GS_ID("general/passport/play_previous_levels"),
            .func = M_HandlePlayPrevLevelSelectSlot,
            .flat = false,
        },
        [M_ROLE_PLAY_PREV_LEVEL_SELECT_LEVEL] = {
            .title = GS_ID("general/passport/play_previous_levels"),
            .func = M_HandlePlayPrevLevelSelectLevel,
            .flat = false,
        },
        [M_ROLE_SWITCH_MOD] = {
            .title = GS_ID("general/passport/switch_mod"),
            .func = M_HandleSwitchMod,
            .flat = false,
        },
        [M_ROLE_RESTART_LEVEL] = {
            .title = GS_ID("general/passport/restart_level"),
            .func = M_HandleRestartLevel,
            .flat = true,
        },
        [M_ROLE_EXIT_GAME] = {
            .title = GS_ID("general/passport/exit_game"),
            .func = M_HandleExitGame,
            .flat = true,
        },
        [M_ROLE_EXIT_TO_TITLE] = {
            .title = GS_ID("general/passport/exit_to_title"),
            .func = M_HandleExitToTitle,
            .flat = true,
        },
        [M_ROLE_STORY_SO_FAR] = {
            .title = GS_ID("general/passport/story_so_far"),
            .func = M_HandleStorySoFar,
            .flat = false,
        },
        [M_ROLE_STORY_SO_FAR_CONFIRM] = {
            .title = GS_ID("general/passport/story_so_far"),
            .func = M_HandleStorySoFarConfirm,
            .flat = false,
        },
    };

    M_PAGE *const page = M_TryGetActivePage();
    if (page == nullptr) {
        return false;
    }
    const M_PAGE_HANDLER *const handler =
        &m_PageHandlers[page->nav.current->role];
    M_ChangePageTextContent(GameString_Get(handler->title));
    if (m_Priv.mode == M_MODE_BROWSE && !handler->flat) {
        if (g_InputDB.menu_confirm) {
            g_Input = (INPUT_STATE) {};
            g_InputDB = (INPUT_STATE) {};
            m_Priv.mode = M_MODE_PICK_OPTION;
            return true;
        }
        return false;
    }
    return handler->func(inv_item);
}

static void M_HandleFlipInputs(void)
{
    bool flipped = false;
    if (g_InputDB.menu_left && M_IsArrowVisible(-1)) {
        for (M_PAGE_NUMBER page = m_Priv.active_page - 1; page >= PAGE_1;
             page--) {
            if (m_Priv.pages[page].available) {
                m_Priv.active_page = page;
                flipped = true;
                break;
            }
        }
    } else if (g_InputDB.menu_right && M_IsArrowVisible(1)) {
        for (M_PAGE_NUMBER page = m_Priv.active_page + 1; page < PAGE_COUNT;
             page++) {
            if (m_Priv.pages[page].available) {
                m_Priv.active_page = page;
                flipped = true;
                break;
            }
        }
    }
    if (flipped) {
        M_ResetNavStack(M_GetActivePage());
    }
}

void Option_Passport_Control(INVENTORY_ITEM *const inv_item, const bool is_busy)
{
    if (m_Priv.active_page == PAGE_UNDETERMINED) {
        M_DeterminePages();
    }

    if (is_busy) {
        if (g_Config.input.enable_responsive_passport) {
            M_HandleFlipInputs();
        }
        return;
    }

    InvRing_RemoveAllText();

    if (M_IsFlipping(inv_item)) {
        return;
    }

    m_Priv.current_page = M_GetCurrentPage(inv_item);
    if (m_Priv.current_page < m_Priv.active_page) {
        M_FlipRight(inv_item);
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    } else if (m_Priv.current_page > m_Priv.active_page) {
        M_FlipLeft(inv_item);
        g_Input = (INPUT_STATE) {};
        g_InputDB = (INPUT_STATE) {};
    } else {
        if (M_ShowPage(inv_item)) {
            // In case of state-changes, apply changes immediately
            M_ShowPage(inv_item);
        }
        M_SyncArrowsVisibility();
        if (g_InputDB.menu_confirm) {
            M_Close(inv_item);
        } else if (g_InputDB.menu_back) {
            if (g_Inv_Mode == INV_DEATH_MODE) {
                g_Input = (INPUT_STATE) {};
                g_InputDB = (INPUT_STATE) {};
            } else {
                M_NavigateOut(inv_item);
            }
        } else {
            M_HandleFlipInputs();
        }
    }
}

void Option_Passport_Draw(INVENTORY_ITEM *const inv_item)
{
    if (m_Priv.mode == M_MODE_BROWSE || M_IsFlipping(inv_item)
        || m_Priv.active_page != m_Priv.current_page) {
        return;
    }

    M_PAGE *const page = M_TryGetActivePage();
    if (page == nullptr) {
        return;
    }
    M_NAV_FRAME *const frame = page->nav.current;
    if (frame->ui.new_game != nullptr) {
        UI_NewGame(frame->ui.new_game);
    }
    if (frame->ui.play_any_level != nullptr) {
        UI_PlayAnyLevelDialog(frame->ui.play_any_level);
    }
    if (frame->ui.select_level != nullptr) {
        UI_SelectLevelDialog(frame->ui.select_level);
    }
    if (frame->ui.switch_mod != nullptr) {
        UI_SwitchModDialog(frame->ui.switch_mod);
    }
    if (frame->ui.save_slot != nullptr) {
        UI_SaveSlotDialog(frame->ui.save_slot);
    }

    if (m_Priv.error_msg != nullptr) {
        UI_BeginModal(0.5f, 0.67f);
        UI_BeginFrame(UI_FRAME_DIALOG_BACKGROUND);
        UI_BeginPad(8.0f, 8.0f);
        UI_Label(GameString_Get(m_Priv.error_msg));
        UI_EndPad();
        UI_EndFrame();
        UI_EndModal();
    }
}

void Option_Passport_Close(void)
{
    M_RemoveAllText();
    M_FreeAllDialogs();
    m_Priv.active_page = PAGE_UNDETERMINED;
}
