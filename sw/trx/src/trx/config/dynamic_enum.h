#pragma once

#include <trx/config/option.h>

#include <stdint.h>

void Config_DynamicEnum_ResetValues(const CONFIG_OPTION *option);
bool Config_DynamicEnum_AddValue(
    const CONFIG_OPTION *option, const char *value, const char *label);
bool Config_DynamicEnum_IsValidValue(
    const CONFIG_OPTION *option, const char *value);
int32_t Config_DynamicEnum_GetValueCount(const CONFIG_OPTION *option);
const char *Config_DynamicEnum_GetValueAt(
    const CONFIG_OPTION *option, int32_t index);
const char *Config_DynamicEnum_GetLabelAt(
    const CONFIG_OPTION *option, int32_t index);
const char *Config_DynamicEnum_GetLabelForValue(
    const CONFIG_OPTION *option, const char *value);
bool Config_DynamicEnum_CanCycle(
    const CONFIG_OPTION *option, const char *current, int32_t dir);
const char *Config_DynamicEnum_GetNext(
    const CONFIG_OPTION *option, const char *current, int32_t dir);
