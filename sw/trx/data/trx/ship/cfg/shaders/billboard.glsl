#define BILLBOARD_LOCK_NONE        0
#define BILLBOARD_LOCK_ROLL        1
#define BILLBOARD_LOCK_ROLL_PITCH  2
#define BILLBOARD_LOCK_PERSPECTIVE 3

vec4 offsetBillboard(vec3 pos, vec2 disp, mat4 view, mat4 model, mat4 proj, int mode)
{
    vec3 right, up;
    if (mode == BILLBOARD_LOCK_NONE) {
        right = normalize(vec3(view[0][0], view[1][0], view[2][0]));
        up    = normalize(vec3(view[0][1], view[1][1], view[2][1]));
    } else {
        // Base forward for all locked modes
        vec3 forward = -normalize(vec3(view[0][2], view[1][2], view[2][2]));
        const vec3 worldUp = vec3(0,1,0);

        if (mode != BILLBOARD_LOCK_ROLL) {
            // Kill pitch if requested by any cylindrical/perspective mode
            forward = normalize(vec3(forward.x, 0.0, forward.z));
        }

        if (mode == BILLBOARD_LOCK_PERSPECTIVE) {
            vec4 clip     = proj * view * model * vec4(pos,1);
            float ndcX    = clip.x / clip.w;
            vec3 yawRight = normalize(cross(forward, worldUp));
            float inv     = inversesqrt(1.0 + ndcX * ndcX);
            forward       = normalize(inv * forward - (ndcX * inv) * yawRight);
        }

        right = normalize(cross(forward, worldUp));
        up    = normalize(cross(right, forward));
    }

    vec4 wp = model * vec4(pos,1);
    wp.xyz += disp.x * right + disp.y * up;
    return view * wp;
}
