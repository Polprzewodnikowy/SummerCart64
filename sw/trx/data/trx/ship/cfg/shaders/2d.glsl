#include "common.glsl"

#define EFFECT_NONE 0
#define EFFECT_VIGNETTE 1
#define EFFECT_WAVE 2

#define WAVE_SPEED_SHORT -3.92
#define WAVE_SPEED_LONG -2.81
#define WAVE_TILE_PHASE_SHORT vec2(67.5, 73.0)
#define WAVE_TILE_PHASE_LONG vec2(33.94, 28.31)
#define WAVE_LIGHT_DELTA 0.125
#define WAVE_Y_TILES 6
#define WAVE_ORBIT_RADIUS 0.2
#define WAVE_FPS_DRIFT 25 / 30

uniform int uEffect;
uniform float uOpacity;
uniform float uBrightnessScale;
uniform int uFitMode;      // 0=stretch,1=letterbox,2=crop,3=smart
uniform float uSrcAspect;  // src_w/src_h

#ifdef VERTEX

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoords;

out vec2 vertCoords;
out float vertLight;
out vec2 vertMappedUv;
out vec4 vertContentRect; // x0,y0,x1,y1 in normalized screen coords

void main() {
    if ((uEffect & EFFECT_WAVE) != 0) {
        float edgeOffset = (1.0 / WAVE_Y_TILES) * 2.0;
        vec2 baseNDC = ((inPosition.xy * (2.0 + 2.0 * edgeOffset)) - (1.0 + edgeOffset)) * vec2(1.0, -1.0);

        vec2 aspectCorrection = vec2(uViewportSize.y / uViewportSize.x, 1);
        vec2 repeat = float(WAVE_Y_TILES) / aspectCorrection;
        float shortPhase = dot(inPosition, repeat * WAVE_TILE_PHASE_SHORT);
        float longPhase = dot(inPosition, repeat * WAVE_TILE_PHASE_LONG);
        float shortAng = radians((uTime * WAVE_FPS_DRIFT)  * WAVE_SPEED_SHORT + shortPhase);
        float longAng = radians((uTime * WAVE_FPS_DRIFT) * WAVE_SPEED_LONG + longPhase);

        float viewportSizeNDC = (1 + edgeOffset * 2);
        vec2 tileSize = viewportSizeNDC / repeat;
        vec2 vertexOffset = vec2(cos(shortAng), sin(shortAng)) * tileSize * WAVE_ORBIT_RADIUS;
        vertLight = 0.5 + (sin(shortAng) + sin(longAng)) * WAVE_LIGHT_DELTA;

        gl_Position  = vec4(baseNDC + vertexOffset, 0.0, 1.0);
    } else {
        vec2 baseNDC = inPosition * vec2(2.0, -2.0) + vec2(-1.0, 1.0);
        gl_Position = vec4(baseNDC, 0.0, 1.0);
    }

    vertCoords    = inPosition;

    int mode = uFitMode;
    float dstAspect = uViewportSize.x / uViewportSize.y;
    float srcAspect = uSrcAspect;

    if (mode == 3) {
        float arDiff =
            (srcAspect > dstAspect ? srcAspect / dstAspect : dstAspect / srcAspect)
            - 1.0;
        if (arDiff <= 0.1) {
            mode = 0;
        } else if (srcAspect <= dstAspect) {
            mode = 1;
        } else {
            mode = 2;
        }
    }

    float x0 = 0.0;
    float y0 = 0.0;
    float x1 = 1.0;
    float y1 = 1.0;

    vec2 uv = inTexCoords;
    if (mode == 1) {
        // Letterbox: compute content rect and map UVs within it.
        if (srcAspect > dstAspect) {
            float h = dstAspect / srcAspect;
            y0 = (1.0 - h) * 0.5;
            y1 = y0 + h;
        } else {
            float w = srcAspect / dstAspect;
            x0 = (1.0 - w) * 0.5;
            x1 = x0 + w;
        }

        uv = (vertCoords - vec2(x0, y0)) / vec2(x1 - x0, y1 - y0);
    } else if (mode == 2) {
        // Crop: keep full screen coverage, but zoom the UVs.
        if (srcAspect < dstAspect) {
            float h = dstAspect / srcAspect;
            float visible = 1.0 / h;
            float v0 = (1.0 - visible) * 0.5;
            uv.y = v0 + uv.y * visible;
        } else {
            float w = srcAspect / dstAspect;
            float visible = 1.0 / w;
            float u0 = (1.0 - visible) * 0.5;
            uv.x = u0 + uv.x * visible;
        }
    }

    vertMappedUv = uv;
    vertContentRect = vec4(x0, y0, x1, y1);
}

#elif defined(FRAGMENT)

uniform sampler2D uTexMain;
uniform vec4 uTexSize;

in vec2 vertCoords;
in float vertLight;
in vec2 vertMappedUv;
in vec4 vertContentRect;
out vec4 outColor;

void main(void) {
    if (vertCoords.x < vertContentRect.x || vertCoords.x > vertContentRect.z
        || vertCoords.y < vertContentRect.y || vertCoords.y > vertContentRect.w) {
        // Outside the content rect: force opaque black so nothing bleeds.
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 uv = clampTexAtlas(vertMappedUv, uTexSize);
    outColor = texture(uTexMain, uv);

    if ((uEffect & EFFECT_WAVE) != 0) {
        outColor.rgb *= vertLight;
    } else if ((uEffect & EFFECT_VIGNETTE) != 0) {
        float x_dist = vertCoords.x - 0.5;
        float y_dist = vertCoords.y - 0.5;
        float lightV = 256.0 - sqrt(x_dist * x_dist + y_dist * y_dist) * 300.0;
        lightV = clamp(lightV, 0.0, 255.0) / 255.0;
        outColor *= vec4(lightV, lightV, lightV, 1.0);
    }

    if (uDesaturation > 0.0) {
        float luma = dot(outColor.rgb, vec3(0.299, 0.587, 0.114));
        outColor.rgb = mix(outColor.rgb, vec3(luma), clamp(uDesaturation, 0.0, 1.0));
    }

    outColor.rgb *= uUIBrightnessMultiplier * uBrightnessScale;

    outColor.a *= clamp(uOpacity, 0.0, 1.0);
    // Output premultiplied alpha so callers can use (ONE, ONE_MINUS_SRC_ALPHA).
    outColor.rgb *= outColor.a;
}

#endif
