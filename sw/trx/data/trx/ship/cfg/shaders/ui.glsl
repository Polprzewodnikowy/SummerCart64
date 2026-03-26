#include "common.glsl"

#ifdef VERTEX

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inUVW;
layout(location = 2) in vec4 inTextureSize;
layout(location = 3) in uint inFlags;
layout(location = 4) in vec4 inColor;

out vec3 gNormal;
flat out uint gFlags;
flat out int gTexLayer;
out vec2 gTexUV;
flat out vec4 gAtlasSize;
out vec4 gColor;

void main(void) {
    gl_Position = uMatProj * uMatView * vec4(inPosition.xyz, 1.0);
    gFlags = inFlags;
    gAtlasSize = inTextureSize;
    gTexUV = inUVW.xy;
    gTexLayer = int(inUVW.z);
    gColor = inColor;
}

#elif defined(FRAGMENT)

uniform sampler2DArray uTexAtlas;

flat in uint gFlags;
flat in int gTexLayer;
in vec2 gTexUV;
flat in vec4 gAtlasSize;
in vec4 gColor;
out vec4 outColor;

void main(void) {
    vec4 texColor = gColor;

    if ((gFlags & VERT_FLAT_SHADED) == 0u && gTexLayer >= 0) {
        vec3 texCoords = vec3(gTexUV.x, gTexUV.y, gTexLayer);
        texCoords.xy = clampTexAtlas(texCoords.xy, gAtlasSize);
        texColor *= texture(uTexAtlas, texCoords);
        if (texColor.a <= 0.0) {
            discard;
        }
    } else {
        texColor.rgb *= texColor.a;
    }

    texColor.rgb *= uUIBrightnessMultiplier;
    outColor = texColor;
}

#endif
