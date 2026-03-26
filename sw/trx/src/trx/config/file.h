#pragma once

#include <trx/config/option.h>
#include <trx/core/enum_map.h>
#include <trx/core/json.h>
#include <trx/core/vector.h>
#include <trx/game/gym.h>

#include <stdint.h>

typedef struct {
    const char *default_path;
    const char *enforced_path;
    void (*action)(JSON_OBJECT *root_obj);
    VECTOR *enforced_targets;
    VECTOR *hidden_targets;
} CONFIG_IO_ARGS;

bool ConfigFile_Read(const CONFIG_IO_ARGS *control);
bool ConfigFile_Write(const CONFIG_IO_ARGS *control);

void ConfigFile_LoadOptions(
    JSON_OBJECT *root_obj, const CONFIG_OPTION *options);
void ConfigFile_DumpOptions(
    JSON_OBJECT *root_obj, const CONFIG_OPTION *options);

int ConfigFile_ReadEnum(
    JSON_OBJECT *obj, const char *name, int default_value,
    const char *enum_name);
void ConfigFile_WriteEnum(
    JSON_OBJECT *obj, const char *name, int value, const char *enum_name);

bool ConfigFile_LoadGymTrackStats(
    JSON_OBJECT *root_obj, const char *key_name,
    GYM_TRACK_STATS *assault_stats);
bool ConfigFile_DumpGymTrackStats(
    JSON_OBJECT *root_obj, const char *key_name,
    const GYM_TRACK_STATS *assault_stats);
