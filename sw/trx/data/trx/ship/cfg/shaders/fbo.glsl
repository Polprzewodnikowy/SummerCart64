#ifdef VERTEX

layout(location = 0) in vec2 inPosition;

out vec2 vertTexCoords;

void main(void) {
    vertTexCoords = inPosition;
    gl_Position = vec4(vertTexCoords * vec2(2.0, 2.0) + vec2(-1.0, -1.0), 0.0, 1.0);
}

#elif defined(FRAGMENT)

uniform sampler2D uTex0;

in vec2 vertTexCoords;
out vec4 outColor;

void main(void) {
    outColor = texture(uTex0, vertTexCoords);
}

#endif
