#define PI 3.1415926538

#define WALL_L      1024
#define WIBBLE_SIZE 32
#define MAX_WIBBLE  2

#define SHADE_NEUTRAL  0x1000
#define SHADE_MAX      0x1FFF
#define SHADE_CAUSTICS 0x300

#define VERT_NO_WIBBLE         0x0001u
#define VERT_FLAT_SHADED       0x0002u
#define VERT_REFLECTIVE        0x0004u
#define VERT_NO_LIGHTING       0x0008u
#define VERT_BILLBOARD         0x0010u
#define VERT_ABS_SPRITE        0x0020u
#define VERT_NO_ALPHA_DISCARD  0x0040u
#define VERT_USE_DYNAMIC_LIGHT 0x0080u
#define VERT_USE_OBJECT_LIGHT  0x0100u
#define VERT_USE_OWN_LIGHT     0x0200u
#define VERT_MOVE              0x0400u
#define VERT_GLOW              0x0800u

#define LIGHTING_CONTRAST_LOW    0
#define LIGHTING_CONTRAST_MEDIUM 1
#define LIGHTING_CONTRAST_HIGH   2

layout(std140) uniform Globals {
    vec4 uGlobalTint;
    vec4 uFogColor;
    vec2 uFogDistance; // x = fog start, y = fog end
    vec2 uViewportSize;
    float uTime;
    float uTimeInGame;
    float uBrightnessMultiplier;
    float uUIBrightnessMultiplier;
    float uGamma;
    float uDesaturation;
    float uSunsetDuration;
    float uMinShade;
    int uBillboardLockMode;
    int uLightingEnabled; // bool
    int uTrapezoidFilterEnabled; // bool
    int uReflectionsEnabled; // bool
    int uTexturesEnabled; // bool
    int uTRVersion;
};

layout(std140) uniform Matrices {
    mat4 uMatProj;
    mat4 uMatView;
};

vec2 clampTexAtlas(vec2 uv, vec4 atlasSize)
{
    float epsilon = 0.5 / 256.0;
    return clamp(uv, atlasSize.xy + epsilon, atlasSize.zw - epsilon);
}
