#define MAX_LIGHTS 32

#define RLM_NORMAL  0
#define RLM_FLICKER 1
#define RLM_GLOW    2
#define RLM_SUNSET  3

uniform int uWaterEffect;
uniform vec3 uWaterEffectParams; // x=choppy amp, y=shimmer amp, z=abs intensity

struct Light {
    vec4 pos;
    vec4 color;
    float shade;
    float falloff;
    float kind;
    float _pad0;
};

layout(std140) uniform Lights {
    int uNumLights;
    int uRoomLightMode;
    Light uLights[MAX_LIGHTS];
};

layout(std140) uniform LightSource {
    float uLightAdder;
    float uLightDivider;
    vec4 uLightVectorSource;
    vec4 uTR3Ambient;
    vec4 uTR3LightDirView[3];
    vec4 uTR3LightColor[3];
};

float ogPhaseTurns(vec3 worldPos, int scheme)
{
    // bucket like OG: xyz * (1/64, 1/64, 1/128)
    ivec3 q = ivec3(floor(vec3(worldPos.x / 64.0,
                               worldPos.y / 64.0,
                               worldPos.z / 128.0)));

    // cheap hash -> 0..1
    float n = fract(sin(
        dot(vec3(q), vec3(12.9898, 78.233, 37.719)) + float(scheme) * 19.19
    ) * 43758.5453);

    // OG-ish: random is multiples of 4, then &63 => 16 lanes
    float lane = floor(n * 16.0);  // 0..15
    float offTurns = lane / 16.0;  // 0,1/16,...15/16

    // time base is uTimeInGame with period 64
    float tTurns = fract(uTimeInGame / 64.0);
    return tTurns + offTurns;
}

float effectChoppy(vec3 worldPos)
{
    int scheme = clamp(uWaterEffect - 2, 0, 21);
    float angle = fract(ogPhaseTurns(worldPos, scheme)) * 2 * PI;
    return -sin(angle) * uWaterEffectParams.x / 2.0;
}

float effectShimmer(vec3 worldPos)
{
    int scheme = clamp(uWaterEffect - 2, 0, 21);
    float angle = fract(ogPhaseTurns(worldPos, scheme)) * 2 * PI;
    return sin(angle) * uWaterEffectParams.y * 8.0;
}

float effectAbs()
{
    return uWaterEffectParams.z * 8.0;
}

int lightFlicker(float t) {
    float h = fract(sin(t * 593.123) * 43758.5453);
    return int(h * 32.0);
}

int lightGlow(float time) {
    float phase = mod(time, 32.0) / 32.0;
    float s = sin(phase * 2 * PI);
    float normalized = (s + 1.0) * 0.5;
    return int(normalized * 31.0);
}

int lightSunset(float time) {
    float sunsetProgress = clamp(time / max(1, uSunsetDuration), 0.0, 1.0);
    return int(sunsetProgress * 31.0);
}

int calcRoomShadeIndex(int mode, float time)
{
    if (mode == RLM_FLICKER) {
        return lightFlicker(time);
    }
    if (mode == RLM_GLOW) {
        return lightGlow(time);
    }
    if (mode == RLM_SUNSET) {
        return lightSunset(time);
    }
    return 0;
}

float lightRoom(
    int lightMode, float time, float vertexPhase)
{
    int i = calcRoomShadeIndex(lightMode, time);
    float j = float(int(vertexPhase) & 31);

    const float MAX_UNIT = 512.0;
    return (j - 16.0) * float(i) * MAX_UNIT / 31.0;
}

float lightWaterCaustics(float shade, vec3 vtxPos)
{
    float time = mod(float(uTimeInGame), float(WIBBLE_SIZE));
    // just a random offset based on the source vertex
    float caustic = fract(sin(dot(vtxPos.xyz, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
    caustic = (caustic * 1023.0) - 511.0;
    float angle = radians(360.0 * mod((time + caustic) / float(WIBBLE_SIZE), 1.0));
    return clamp(shade + sin(angle) * float(SHADE_CAUSTICS), 0.0, float(SHADE_MAX));
}

vec3 safeNormalize(vec3 v)
{
    float len2 = dot(v, v);
    if (len2 <= 0.0) {
        return vec3(0.0);
    }
    return v * inversesqrt(len2);
}

float lightObjects(vec3 rawNormal, vec4 vertexPos)
{
    float lightAdder = uLightAdder;
    if (uLightDivider != 0) {
        vec3 L = mat3(transpose(uMatView * uMatModel)) * uLightVectorSource.xyz / uLightDivider;
        lightAdder += dot(L, rawNormal.xyz / (1 << 14)) / 4;
        lightAdder = clamp(lightAdder, 0, SHADE_MAX);
    }
    return lightAdder;
}

vec3 lightObjectsTR3(vec3 rawNormal)
{
    vec3 N = safeNormalize(mat3(uMatView * uMatModel) * (rawNormal.xyz / float(1 << 14)));

    vec3 L0 = uTR3LightDirView[0].xyz;
    vec3 L1 = uTR3LightDirView[1].xyz;
    vec3 L2 = uTR3LightDirView[2].xyz;

    float d0 = max(dot(N, L0), 0.0);
    float d1 = max(dot(N, L1), 0.0);
    float d2 = max(dot(N, L2), 0.0);

    vec3 rgb = uTR3Ambient.rgb
        + uTR3LightColor[0].rgb * d0
        + uTR3LightColor[1].rgb * d1
        + uTR3LightColor[2].rgb * d2;
    return clamp(rgb, 0.0, 1.0);
}

vec3 lightOwnTR3(float shade)
{
    float shade8 = floor((SHADE_MAX - shade) / 32.0); // (0x1FFF - shade) >> 5
    shade8 = (shade8 <= 0.0) ? 255.0 : shade8;
    return clamp(uTR3Ambient.rgb * (shade8 / 255.0), 0.0, 1.0);
}

float lightDynamicTR12Lum(float baseLight, vec4 vertexPos)
{
    float lightAdder = baseLight;
    for (int i = 0; i < uNumLights; i++) {
        if (uLights[i].kind != 0.0) {
            continue;
        }
        vec3 dist = uLights[i].pos.xyz - vertexPos.xyz;
        float radius = exp2(uLights[i].falloff);
        float distSq = dot(dist, dist);
        if (distSq > radius * radius) {
            continue;
        }

        float maxShade = exp2(uLights[i].shade);
        float distTerm = distSq / exp2(2 * uLights[i].falloff - uLights[i].shade);
        float shade = maxShade - distTerm;
        lightAdder -= shade;
    }
    return max(lightAdder, 0);
}

vec3 lightDynamicTR12RGB(vec4 vertexPos)
{
    vec3 add = vec3(0.0);
    for (int i = 0; i < uNumLights; i++) {
        if (uLights[i].kind == 0.0) {
            continue;
        }

        float radius = uLights[i].falloff * 0.5;
        vec3 dist = uLights[i].pos.xyz - vertexPos.xyz;
        float distSq = dot(dist, dist);
        float radiusSq = radius * radius;
        if (distSq > radiusSq) {
            continue;
        }

        float d = sqrt(distSq);
        float factor = (radius - d) / max(radius, 1.0);
        add += factor * uLights[i].color.rgb;
    }
    return add;
}

vec3 lightDynamicTR3(vec4 vertexPos)
{
    vec3 add = vec3(0.0);
    for (int i = 0; i < uNumLights; i++) {
        float radius = uLights[i].falloff * 0.5; // falloff_raw >> 1
        vec3 dist = uLights[i].pos.xyz - vertexPos.xyz;
        float distSq = dot(dist, dist);
        float radiusSq = radius * radius;
        if (distSq > radiusSq) {
            continue;
        }

        float d = sqrt(distSq);
        float factor = (radius - d) / max(radius, 1.0);
        add += factor * uLights[i].color.rgb;
    }
    return add;
}

float getDynamicLightContrastMul()
{
    // `uMinShade` is configured via the "lighting contrast" option.
    // For TR1/TR2 it clamps the minimum shade; in TR3 the lighting is additive,
    // so we remap it to a multiplier:
    // LOW: uMinShade = SHADE_NEUTRAL -> 1.0
    // MED: uMinShade = SHADE_HIGH    -> 1.5
    // HIGH:uMinShade = 0             -> 2.0
    return clamp(2.0 - (uMinShade / float(SHADE_NEUTRAL)), 1.0, 2.0);
}

float lightLumTR12(float shade, uint flags, vec3 normal, vec4 pos, float phase)
{
    if ((flags & VERT_USE_OWN_LIGHT) != 0u) {
        shade = uLightAdder + shade;
    } else if ((flags & VERT_USE_OBJECT_LIGHT) != 0u) {
        shade = lightObjects(normal, pos);
    } else {
        if ((flags & VERT_USE_DYNAMIC_LIGHT) != 0u) {
            shade = lightDynamicTR12Lum(shade, pos);
            shade += lightRoom(uRoomLightMode, uTimeInGame, phase);
        }
        shade = clamp(shade, 0, SHADE_MAX);
    }

    if (uWaterEffect == 1) {
        shade = lightWaterCaustics(shade, pos.xyz);
    }

    return shade;
}

struct LightingResult {
    float shade; // used only for TR1-2
    vec3 add; // TR3: additive light (dynamic + post effects)
    vec3 mul; // TR3: multiplicative light (object/own)
};

LightingResult light(
    float shade, uint flags, vec3 normal, vec4 pos,
    float vertexPhase)
{
    LightingResult result;
    result.shade = SHADE_NEUTRAL;
    result.add = vec3(0.0);
    result.mul = vec3(1.0);

    if (uLightingEnabled == 0) {
        return result;
    }
    if ((flags & VERT_NO_LIGHTING) != 0u) {
        return result;
    }

#if TR_VERSION >= 3
    if ((flags & VERT_USE_DYNAMIC_LIGHT) != 0u) {
        result.add += lightDynamicTR3(pos) * getDynamicLightContrastMul();
    }

    if ((flags & VERT_USE_OBJECT_LIGHT) != 0u) {
        result.mul *= lightObjectsTR3(normal);
    } else if ((flags & VERT_USE_OWN_LIGHT) != 0u) {
        result.mul *= lightOwnTR3(shade);
    }

    float add = 0.0;
    if ((flags & VERT_MOVE) != 0u) {
        add += effectChoppy(pos.xyz) / 256.0;
    }
    if ((flags & VERT_GLOW) != 0u) {
        add += effectShimmer(pos.xyz) / 256.0;
        add += effectAbs() / 256.0;
    }
    result.add += vec3(add);

    result.shade = SHADE_NEUTRAL;
#else
    result.shade = lightLumTR12(shade, flags, normal, pos, vertexPhase);
    if ((flags & VERT_USE_DYNAMIC_LIGHT) != 0u) {
        result.add += lightDynamicTR12RGB(pos) * getDynamicLightContrastMul();
    }
#endif

    return result;
}
