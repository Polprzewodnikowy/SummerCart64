#include <trx/core/vector.h>

#define ENUM_MAP(enum_type_name, enum_value, str_value)                        \
    EnumMap_Define(                                                            \
        ENUM_MAP_NAME(enum_type_name), #enum_value,                            \
        ENUM_MAP_LABEL_KEY(enum_type_name, enum_value), enum_value, str_value)

#define ENUM_MAP_SELF(enum_type_name, enum_value)                              \
    ENUM_MAP(enum_type_name, enum_value, #enum_value)

#define ENUM_MAP_GET(enum_type_name, str_value, default_value)                 \
    EnumMap_Get(ENUM_MAP_NAME(enum_type_name), str_value, default_value)

#define ENUM_MAP_TO_STRING(enum_type_name, enum_value)                         \
    EnumMap_ToString(ENUM_MAP_NAME(enum_type_name), enum_value)

#define ENUM_MAP_NAME(enum_type_name) #enum_type_name
#define ENUM_MAP_LABEL_KEY(enum_type_name, enum_value)                         \
    "enums/" #enum_type_name "/" #enum_value

// Associate an integer enum value, such as WEATHER_SNOW, with a string
// representation such as "snow".
// @param enum_type_name    Name of the enum type, such as "WEATHER".
// @param enum_name         Name of the enum, such as "WEATHER_SNOW".
// @param enum_value        Value of the enum, such as 1.
// @param str_value         String representation of the enum, such as "snow".
void EnumMap_Define(
    const char *enum_type_name, const char *enum_name, const char *label_key,
    int32_t enum_value, const char *str_value);

// Retrieve an integer enum value from a string representation.
// @param enum_type_name    Name of the enum type, such as "WEATHER".
// @param str_value         String representation of the enum, such as "snow".
// @param default_value     Value to return in case the mapping fails.
// @return                  Value of the enum, such as 1.
int32_t EnumMap_Get(
    const char *enum_type_name, const char *str_value, int32_t default_value);

// Retrieve an enum integer canonical name as a string.
// @param enum_type_name    Name of the enum type, such as "WEATHER".
// @param enum_value        Value of the enum, such as 1.
// @return                  Name of the enum, such as "WEATHER_SNOW".
const char *EnumMap_GetName(const char *enum_type_name, int32_t enum_value);

// Retrieve a string representation, such as "snow", based on an integer enum
// value such as WEATHER_SNOW.
// @param enum_type_name    Name of the enum type, such as "WEATHER".
// @param enum_value        Value of the enum, such as 1.
// @return                  String representation of the enum, such as "snow".
const char *EnumMap_ToString(const char *enum_type_name, int32_t enum_value);

// Retrieve a localized label for the given enum value.
// @param enum_type_name    Name of the enum type, such as "WEATHER".
// @param enum_value        Value of the enum, such as 1.
// @return                  Localized label or nullptr if missing.
const char *EnumMap_GetLabel(const char *enum_type_name, int32_t enum_value);

// Returns a vector of valid string values for the given enum_type_name.
//
// The returned vector must be freed via Vector_Free(). The string pointers
// within the vector are owned by the enum map and should not be freed by the
// caller. Returns nullptr if the enum_type_name is not valid.
VECTOR *EnumMap_ListValues(const char *enum_type_name);
