/*
 * MW-N64: Spell and magic effects system
 *
 * INTEGRITY RULE: All effect IDs, formulas, and mechanics from OpenMW
 * (apps/openmw/mwmechanics/spellcasting.cpp, magiceffects.cpp).
 *
 * Morrowind spells: effect_id + range + area + duration + magnitude
 * Resist: (1.0 - resist/100) applied to magnitude
 * Spell success: willpower/10 + spell_type_skill - spell_cost/2
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_character.h"
#include <stdint.h>

/* Effect IDs (from OpenMW components/esm3/effectlist.hpp) */
#define EFX_WATER_BREATHING     0
#define EFX_SWIFT_SWIM          1
#define EFX_WATER_WALKING       2
#define EFX_SHIELD              3
#define EFX_FIRE_SHIELD         4
#define EFX_LIGHTNING_SHIELD    5
#define EFX_FROST_SHIELD        6
#define EFX_BURDEN              7
#define EFX_FEATHER             8
#define EFX_JUMP                9
#define EFX_LEVITATE            10
#define EFX_SLOW_FALL           11
#define EFX_LOCK                12
#define EFX_OPEN                13
#define EFX_FIRE_DAMAGE         14
#define EFX_SHOCK_DAMAGE        15
#define EFX_FROST_DAMAGE        16
#define EFX_DRAIN_ATTRIBUTE     17
#define EFX_DRAIN_HEALTH        18
#define EFX_DRAIN_MAGICKA       19
#define EFX_DRAIN_FATIGUE       20
#define EFX_DRAIN_SKILL         21
#define EFX_DAMAGE_ATTRIBUTE    22
#define EFX_DAMAGE_HEALTH       23
#define EFX_DAMAGE_MAGICKA      24
#define EFX_DAMAGE_FATIGUE      25
#define EFX_DAMAGE_SKILL        26
#define EFX_POISON              27
#define EFX_WEAKNESS_FIRE       28
#define EFX_WEAKNESS_FROST      29
#define EFX_WEAKNESS_SHOCK      30
#define EFX_WEAKNESS_MAGICKA    31
#define EFX_WEAKNESS_DISEASE    32
#define EFX_WEAKNESS_BLIGHT     33
#define EFX_WEAKNESS_POISON     34
#define EFX_WEAKNESS_NORMAL     35
#define EFX_DISINTEGRATE_WEAPON 36
#define EFX_DISINTEGRATE_ARMOR  37
#define EFX_INVISIBILITY        38
#define EFX_CHAMELEON           39
#define EFX_LIGHT               40
#define EFX_SANCTUARY           41
#define EFX_NIGHT_EYE           42
#define EFX_CHARM               43
#define EFX_PARALYZE            44
#define EFX_SILENCE             45
#define EFX_BLIND               46
#define EFX_SOUND               47
#define EFX_CALM_HUMANOID       48
#define EFX_CALM_CREATURE       49
#define EFX_FRENZY_HUMANOID     50
#define EFX_FRENZY_CREATURE     51
#define EFX_DEMORALIZE_HUMANOID 52
#define EFX_DEMORALIZE_CREATURE 53
#define EFX_RALLY_HUMANOID      54
#define EFX_RALLY_CREATURE      55
#define EFX_DISPEL              56
#define EFX_SOULTRAP            57
#define EFX_TELEKINESIS         58
#define EFX_MARK                59
#define EFX_RECALL              60
#define EFX_DIVINE_INTERVENTION 61
#define EFX_ALMSIVI_INTERVENTION 62
#define EFX_DETECT_ANIMAL       63
#define EFX_DETECT_ENCHANTMENT  64
#define EFX_DETECT_KEY          65
#define EFX_SPELL_ABSORPTION    66
#define EFX_REFLECT             67
#define EFX_CURE_DISEASE        68
#define EFX_CURE_BLIGHT         69
#define EFX_CURE_POISON         70
#define EFX_CURE_PARALYZATION   71
#define EFX_RESTORE_ATTRIBUTE   72
#define EFX_RESTORE_HEALTH      73
#define EFX_RESTORE_MAGICKA     74
#define EFX_RESTORE_FATIGUE     75
#define EFX_RESTORE_SKILL       76
#define EFX_FORTIFY_ATTRIBUTE   77
#define EFX_FORTIFY_HEALTH      78
#define EFX_FORTIFY_MAGICKA     79
#define EFX_FORTIFY_FATIGUE     80
#define EFX_FORTIFY_SKILL       81
#define EFX_FORTIFY_MAX_MAGICKA 82
#define EFX_ABSORB_ATTRIBUTE    83
#define EFX_ABSORB_HEALTH       84
#define EFX_ABSORB_MAGICKA      85
#define EFX_ABSORB_FATIGUE      86
#define EFX_ABSORB_SKILL        87
#define EFX_RESIST_FIRE         88
#define EFX_RESIST_FROST        89
#define EFX_RESIST_SHOCK        90
#define EFX_RESIST_MAGICKA      91
#define EFX_RESIST_DISEASE      92
#define EFX_RESIST_BLIGHT       93
#define EFX_RESIST_POISON       94
#define EFX_RESIST_NORMAL       95
#define EFX_RESIST_PARALYSIS    96
#define EFX_REMOVE_CURSE        97
#define EFX_TURN_UNDEAD         98
#define EFX_SUMMON_SCAMP        99
/* ... continues to ~142 effects */
#define EFX_COUNT               143

/* Spell effect instance */
typedef struct {
    int16_t  effect_id;
    int16_t  skill;         /* For skill-targeting effects */
    int16_t  attribute;     /* For attribute-targeting effects */
    int32_t  magnitude_min;
    int32_t  magnitude_max;
    int32_t  duration;      /* Seconds */
    int32_t  area;          /* Radius */
    uint8_t  range;         /* 0=self, 1=touch, 2=target */
} spell_effect_t;

/* Spell definition */
typedef struct {
    char           name[64];
    int32_t        cost;
    uint32_t       flags;   /* 0x1=autocalc, 0x2=PC start, 0x4=always succeeds */
    int32_t        type;    /* 0=spell, 1=ability, 2=blight, 3=disease, 4=curse, 5=power */
    spell_effect_t effects[8];
    int            effect_count;
} mw_spell_t;

/*
 * Spell success chance formula
 * From OpenMW mwmechanics/spellcasting.cpp getSpellSuccessChance:
 *
 *   chance = (skill * 2 + willpower/5 + luck/10 - cost * fSpellMakingValueMult)
 *            * fatigue_term
 *
 * Where skill = the school skill (destruction, alteration, etc.)
 * fSpellMakingValueMult = 1.0 (GMST)
 */
int32_t spell_success_chance(const mw_player_t *caster,
                              const mw_spell_t *spell,
                              int32_t school_skill) {
    /* Always-succeed spells (abilities, diseases, etc.) */
    if (spell->flags & 0x4) return 100;
    if (spell->type != 0) return 100;  /* Only cast spells can fail */

    int32_t willpower = caster->attributes[ATTR_WILLPOWER];
    int32_t luck = caster->attributes[ATTR_LUCK];

    int32_t chance = school_skill * 2 + willpower / 5 + luck / 10
                     - spell->cost;

    /* Fatigue term */
    int32_t fat_term = combat_fatigue_term(caster->fatigue, caster->max_fatigue);
    chance = (chance * fat_term) >> 16;

    if (chance < 0) chance = 0;
    if (chance > 100) chance = 100;
    return chance;
}

/*
 * Apply a spell effect to a target.
 * From OpenMW mwmechanics/spellcasting.cpp applyMagicEffect.
 */
void spell_apply_effect(mw_player_t *target, const spell_effect_t *effect,
                         int32_t magnitude) {
    /* Apply resist */
    int32_t resist = 0;
    switch (effect->effect_id) {
        case EFX_FIRE_DAMAGE: resist = 0; break; /* TODO: look up fire resist */
        case EFX_FROST_DAMAGE: resist = 0; break;
        case EFX_SHOCK_DAMAGE: resist = 0; break;
        default: break;
    }
    magnitude = (magnitude * (100 - resist)) / 100;
    if (magnitude <= 0) return;

    switch (effect->effect_id) {
        case EFX_RESTORE_HEALTH:
            target->health += magnitude;
            if (target->health > target->max_health) target->health = target->max_health;
            break;
        case EFX_RESTORE_MAGICKA:
            target->magicka += magnitude;
            if (target->magicka > target->max_magicka) target->magicka = target->max_magicka;
            break;
        case EFX_RESTORE_FATIGUE:
            target->fatigue += magnitude;
            if (target->fatigue > target->max_fatigue) target->fatigue = target->max_fatigue;
            break;
        case EFX_DAMAGE_HEALTH:
        case EFX_FIRE_DAMAGE:
        case EFX_FROST_DAMAGE:
        case EFX_SHOCK_DAMAGE:
        case EFX_POISON:
            target->health -= magnitude;
            break;
        case EFX_DAMAGE_MAGICKA:
        case EFX_DRAIN_MAGICKA:
            target->magicka -= magnitude;
            if (target->magicka < 0) target->magicka = 0;
            break;
        case EFX_DAMAGE_FATIGUE:
        case EFX_DRAIN_FATIGUE:
            target->fatigue -= magnitude;
            if (target->fatigue < 0) target->fatigue = 0;
            break;
        case EFX_FORTIFY_ATTRIBUTE:
            if (effect->attribute >= 0 && effect->attribute < ATTR_COUNT) {
                target->attributes[effect->attribute] += magnitude;
            }
            break;
        case EFX_DRAIN_ATTRIBUTE:
        case EFX_DAMAGE_ATTRIBUTE:
            if (effect->attribute >= 0 && effect->attribute < ATTR_COUNT) {
                target->attributes[effect->attribute] -= magnitude;
                if (target->attributes[effect->attribute] < 0)
                    target->attributes[effect->attribute] = 0;
            }
            break;
        default:
            /* TODO: many more effects (levitate, paralyze, etc.) */
            break;
    }
}
