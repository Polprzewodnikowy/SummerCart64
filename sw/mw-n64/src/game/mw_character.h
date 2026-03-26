/*
 * MW-N64: Character system
 *
 * Implements Morrowind's character creation, attributes, skills,
 * leveling, and combat formulas. All values are exact — taken from
 * GMST (Game Setting) records in the ESM and documented formulas.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MW_CHARACTER_H
#define MW_CHARACTER_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------
 * Attributes (8 total — exact Morrowind names and order)
 * ------------------------------------------------------------------- */

typedef enum {
    ATTR_STRENGTH = 0,
    ATTR_INTELLIGENCE = 1,
    ATTR_WILLPOWER = 2,
    ATTR_AGILITY = 3,
    ATTR_SPEED = 4,
    ATTR_ENDURANCE = 5,
    ATTR_PERSONALITY = 6,
    ATTR_LUCK = 7,
    ATTR_COUNT = 8,
} attribute_t;

/* -------------------------------------------------------------------
 * Skills (27 total — exact Morrowind names and order)
 * ------------------------------------------------------------------- */

typedef enum {
    SKILL_BLOCK = 0,
    SKILL_ARMORER = 1,
    SKILL_MEDIUM_ARMOR = 2,
    SKILL_HEAVY_ARMOR = 3,
    SKILL_BLUNT_WEAPON = 4,
    SKILL_LONG_BLADE = 5,
    SKILL_AXE = 6,
    SKILL_SPEAR = 7,
    SKILL_ATHLETICS = 8,
    SKILL_ENCHANT = 9,
    SKILL_DESTRUCTION = 10,
    SKILL_ALTERATION = 11,
    SKILL_ILLUSION = 12,
    SKILL_CONJURATION = 13,
    SKILL_MYSTICISM = 14,
    SKILL_RESTORATION = 15,
    SKILL_ALCHEMY = 16,
    SKILL_UNARMORED = 17,
    SKILL_SECURITY = 18,
    SKILL_SNEAK = 19,
    SKILL_ACROBATICS = 20,
    SKILL_LIGHT_ARMOR = 21,
    SKILL_SHORT_BLADE = 22,
    SKILL_MARKSMAN = 23,
    SKILL_MERCANTILE = 24,
    SKILL_SPEECHCRAFT = 25,
    SKILL_HAND_TO_HAND = 26,
    SKILL_COUNT = 27,
} skill_t;

/* Skill type: determines which attribute governs the skill */
typedef enum {
    SKILL_TYPE_MAJOR = 0,
    SKILL_TYPE_MINOR = 1,
    SKILL_TYPE_MISC = 2,
} skill_type_t;

/* -------------------------------------------------------------------
 * Player character state
 * ------------------------------------------------------------------- */

typedef struct {
    /* Identity */
    char     name[64];
    char     race[64];
    char     class_name[64];
    char     birthsign[64];
    uint8_t  gender;        /* 0=male, 1=female */

    /* Attributes (current / base) */
    int32_t  attributes[ATTR_COUNT];
    int32_t  base_attributes[ATTR_COUNT];

    /* Skills (current / base) */
    int32_t  skills[SKILL_COUNT];
    int32_t  base_skills[SKILL_COUNT];
    float    skill_progress[SKILL_COUNT]; /* Progress to next level-up */

    /* Major/minor/misc skill classification */
    skill_type_t skill_types[SKILL_COUNT];

    /* Derived stats */
    int32_t  health;
    int32_t  max_health;
    int32_t  magicka;
    int32_t  max_magicka;
    int32_t  fatigue;
    int32_t  max_fatigue;
    int32_t  level;

    /* Level-up tracking: which attributes increased this level */
    int32_t  skill_increases[ATTR_COUNT]; /* Per-attribute skill-up count */
    int32_t  level_progress;             /* Major/minor skill-ups toward next level */

    /* Faction memberships */
    struct {
        char name[64];
        int8_t rank;
    } factions[16];
    int faction_count;

    /* Inventory (simplified — items by ID + count) */
    struct {
        char id[64];
        int32_t count;
    } inventory[256];
    int inventory_count;

    /* Equipment slots */
    char equipped[10][64]; /* Head, chest, legs, feet, hands, etc. */

    /* Active effects */
    struct {
        int16_t effect_id;
        int32_t magnitude;
        int32_t duration;   /* Frames remaining */
    } active_effects[32];
    int active_effect_count;

    /* World position */
    float    pos_x, pos_y, pos_z;
    float    rot_x, rot_y, rot_z;
    char     current_cell[64];
    int32_t  cell_x, cell_y;
    bool     in_interior;
} mw_player_t;

/* -------------------------------------------------------------------
 * Combat formulas (exact Morrowind values)
 *
 * Hit chance = (weapon_skill + attack_bonus + agility/5 + luck/10
 *              - target_evasion) * fatigue_term
 *
 * Damage = base_weapon_damage * strength_modifier
 *
 * All constants from GMST records.
 * ------------------------------------------------------------------- */

/* Calculate melee hit chance (0-100) */
int32_t combat_hit_chance(const mw_player_t *attacker,
                           int32_t weapon_skill,
                           int32_t target_agility,
                           int32_t target_luck,
                           int32_t target_sanctuary);

/* Calculate weapon damage */
int32_t combat_damage(int32_t base_min, int32_t base_max,
                       int32_t strength, int32_t weapon_condition);

/* Calculate fatigue term (0.0-1.0 as 16.16 fixed) */
int32_t combat_fatigue_term(int32_t current_fatigue, int32_t max_fatigue);

/* -------------------------------------------------------------------
 * Leveling (exact Morrowind algorithm)
 *
 * Skill-up: when skill progress >= threshold
 *   threshold depends on skill type (major=lower, misc=higher)
 * Level-up: when 10 major/minor skill increases accumulated
 *   attribute bonuses: 1-5× based on skill_increases per attribute
 * ------------------------------------------------------------------- */

/* Process a skill increase (called when skill XP threshold reached) */
void char_skill_increase(mw_player_t *pc, skill_t skill);

/* Check if player can level up */
bool char_can_level_up(const mw_player_t *pc);

/* Apply level up with chosen attribute increases */
void char_level_up(mw_player_t *pc, attribute_t attr1, attribute_t attr2, attribute_t attr3);

/* Calculate max health: base_health + (endurance + strength) / 2 × level */
void char_recalc_derived(mw_player_t *pc);

#endif /* MW_CHARACTER_H */
