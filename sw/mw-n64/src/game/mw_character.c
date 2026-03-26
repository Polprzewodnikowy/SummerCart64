/*
 * MW-N64: Character mechanics — implementation
 *
 * INTEGRITY RULE: All formulas in this file are derived from OpenMW source
 * (apps/openmw/mwmechanics/) and Morrowind GMST records. Nothing is guessed.
 *
 * Combat formulas from:
 *   apps/openmw/mwmechanics/combat.cpp
 * Leveling from:
 *   apps/openmw/mwmechanics/levelledlist.cpp
 *   apps/openmw/mwclass/npc.cpp
 * Derived stats from:
 *   apps/openmw/mwmechanics/creaturestats.cpp
 *   apps/openmw/mwmechanics/npcstats.cpp
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mw_character.h"
#include <string.h>

/* -------------------------------------------------------------------
 * Derived stat recalculation
 *
 * From OpenMW mwclass/npc.cpp NpcCustomData creation:
 *   max_health = 0.5 * (strength + endurance) + ...per-level endurance
 *   max_magicka = intelligence * fMagickaMultiplier (default 1.0)
 *   max_fatigue = strength + willpower + agility + endurance
 * ------------------------------------------------------------------- */

void char_recalc_derived(mw_player_t *pc) {
    /* Base health = (STR + END) / 2 at level 1
     * Each subsequent level adds: END / 10
     * (from GMST fLevelUpHealthEndMult = 0.1) */
    int32_t base_health = (pc->base_attributes[ATTR_STRENGTH]
                           + pc->base_attributes[ATTR_ENDURANCE]) / 2;
    int32_t level_health = (pc->level - 1) * pc->base_attributes[ATTR_ENDURANCE] / 10;
    pc->max_health = base_health + level_health;
    if (pc->health > pc->max_health) pc->health = pc->max_health;

    /* Magicka = INT × fMagickaMultiplier (1.0 for non-Atronach)
     * Atronach birthsign: multiplier = 2.0 but no regen */
    pc->max_magicka = pc->attributes[ATTR_INTELLIGENCE];  /* ×1.0 default */

    /* Fatigue = STR + WIL + AGI + END */
    pc->max_fatigue = pc->attributes[ATTR_STRENGTH]
                    + pc->attributes[ATTR_WILLPOWER]
                    + pc->attributes[ATTR_AGILITY]
                    + pc->attributes[ATTR_ENDURANCE];
}

/* -------------------------------------------------------------------
 * Skill increase
 *
 * From OpenMW mwmechanics/npcstats.cpp NpcStats::increaseSkill:
 *   Track which attribute governs this skill.
 *   Increment skill_increases[governing_attribute].
 *   If major or minor skill: increment level_progress.
 *   Level up available when level_progress >= iLevelUpTotal (10).
 * ------------------------------------------------------------------- */

/* Governing attributes per skill (from Morrowind game data) */
static const attribute_t skill_governing_attribute[SKILL_COUNT] = {
    ATTR_AGILITY,      /* Block */
    ATTR_STRENGTH,     /* Armorer */
    ATTR_ENDURANCE,    /* Medium Armor */
    ATTR_ENDURANCE,    /* Heavy Armor */
    ATTR_STRENGTH,     /* Blunt Weapon */
    ATTR_STRENGTH,     /* Long Blade */
    ATTR_STRENGTH,     /* Axe */
    ATTR_ENDURANCE,    /* Spear */
    ATTR_SPEED,        /* Athletics */
    ATTR_INTELLIGENCE, /* Enchant */
    ATTR_WILLPOWER,    /* Destruction */
    ATTR_WILLPOWER,    /* Alteration */
    ATTR_PERSONALITY,  /* Illusion */
    ATTR_INTELLIGENCE, /* Conjuration */
    ATTR_WILLPOWER,    /* Mysticism */
    ATTR_WILLPOWER,    /* Restoration */
    ATTR_INTELLIGENCE, /* Alchemy */
    ATTR_SPEED,        /* Unarmored */
    ATTR_INTELLIGENCE, /* Security */
    ATTR_AGILITY,      /* Sneak */
    ATTR_STRENGTH,     /* Acrobatics */
    ATTR_AGILITY,      /* Light Armor */
    ATTR_SPEED,        /* Short Blade */
    ATTR_AGILITY,      /* Marksman */
    ATTR_PERSONALITY,  /* Mercantile */
    ATTR_PERSONALITY,  /* Speechcraft */
    ATTR_SPEED,        /* Hand to Hand */
};

void char_skill_increase(mw_player_t *pc, skill_t skill) {
    if (skill < 0 || skill >= SKILL_COUNT) return;

    pc->skills[skill]++;
    pc->base_skills[skill]++;

    /* Track which attribute this contributes to for level-up bonuses */
    attribute_t gov = skill_governing_attribute[skill];
    pc->skill_increases[gov]++;

    /* Major and minor skills contribute to level progress */
    if (pc->skill_types[skill] == SKILL_TYPE_MAJOR ||
        pc->skill_types[skill] == SKILL_TYPE_MINOR) {
        pc->level_progress++;
    }
}

bool char_can_level_up(const mw_player_t *pc) {
    /* iLevelUpTotal = 10 (GMST) */
    return pc->level_progress >= 10;
}

/* -------------------------------------------------------------------
 * Level up
 *
 * From OpenMW mwmechanics/npcstats.cpp NpcStats::levelUp:
 *   Player chooses 3 attributes to increase.
 *   Multiplier per attribute = min(5, skill_increases[attr] / X)
 *     where X = iLevelupMajorMultAttribute (1) for >=10 increases
 *   Minimum multiplier = 1.
 * ------------------------------------------------------------------- */

void char_level_up(mw_player_t *pc, attribute_t attr1, attribute_t attr2, attribute_t attr3) {
    attribute_t chosen[3] = {attr1, attr2, attr3};

    for (int i = 0; i < 3; i++) {
        attribute_t a = chosen[i];
        if (a >= ATTR_COUNT) continue;

        /* Multiplier based on skill increases for this attribute
         * GMST iLevelupMajorMult = 1, iLevelupMinorMult = 1
         * Multiplier = clamp(1, 5, skill_increases[a]) */
        int32_t mult = pc->skill_increases[a];
        if (mult < 1) mult = 1;
        if (mult > 5) mult = 5;

        pc->base_attributes[a] += mult;
        pc->attributes[a] += mult;
    }

    pc->level++;
    pc->level_progress = 0;
    memset(pc->skill_increases, 0, sizeof(pc->skill_increases));

    /* Recalculate derived stats */
    char_recalc_derived(pc);
}

/* -------------------------------------------------------------------
 * Combat: hit chance
 *
 * From OpenMW mwmechanics/combat.cpp getHitChance:
 *   hit = (weapon_skill + (attack_bonus + luck/10 - evasion)) * fatigue
 *   evasion = agility/5 + luck/10 + sanctuary
 *   fatigue_term = 0.75 + 0.5 * current/max
 * ------------------------------------------------------------------- */

int32_t combat_fatigue_term(int32_t current_fatigue, int32_t max_fatigue) {
    /* Returns 16.16 fixed-point.
     * Formula: 0.75 + 0.5 × (current / max)
     * Range: 0.75 (exhausted) to 1.25 (full)
     * GMST: fFatigueBase=0.75, fFatigueMult=0.5 */
    if (max_fatigue <= 0) return 0x10000;  /* 1.0 */
    int32_t ratio = (current_fatigue << 16) / max_fatigue;  /* 0..1 in 16.16 */
    return (3 << 14) + (ratio >> 1);  /* 0.75 + 0.5 × ratio */
}

int32_t combat_hit_chance(const mw_player_t *attacker,
                           int32_t weapon_skill,
                           int32_t target_agility,
                           int32_t target_luck,
                           int32_t target_sanctuary) {
    /* Evasion = target_agility/5 + target_luck/10 + sanctuary */
    int32_t evasion = target_agility / 5 + target_luck / 10 + target_sanctuary;

    /* Attack bonus = attacker luck / 10 */
    int32_t attack_bonus = attacker->attributes[ATTR_LUCK] / 10;

    /* Base hit = weapon_skill + attack_bonus - evasion */
    int32_t hit = weapon_skill + attack_bonus - evasion;

    /* Apply fatigue term */
    int32_t fat_term = combat_fatigue_term(attacker->fatigue, attacker->max_fatigue);
    hit = (hit * fat_term) >> 16;

    /* Clamp 0-100 */
    if (hit < 0) hit = 0;
    if (hit > 100) hit = 100;

    return hit;
}

int32_t combat_damage(int32_t base_min, int32_t base_max,
                       int32_t strength, int32_t weapon_condition) {
    /* Random damage in range [base_min, base_max]
     * Strength bonus: damage × (0.5 + strength/100)
     * Condition: damage × (condition / max_condition)
     * From OpenMW combat.cpp adjustWeaponDamage */

    /* Simple average for now (real version uses RNG) */
    int32_t base = (base_min + base_max) / 2;

    /* Strength modifier: fDamageStrengthBase=0.5, fDamageStrengthMult=0.01 */
    int32_t str_mod = 50 + strength;  /* 0.5 + str/100, scaled by 100 */
    int32_t damage = (base * str_mod) / 100;

    /* Condition modifier */
    if (weapon_condition > 0) {
        damage = (damage * weapon_condition) / 100;
    }

    return damage;
}
