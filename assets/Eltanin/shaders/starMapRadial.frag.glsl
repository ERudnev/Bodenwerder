#version 460 core
in vec3 vLocalPos;
out vec4 FragColor;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

void main() {
    vec2 p = vLocalPos.xz;
    float r = length(p);
    float fw = max(fwidth(r) * 1.5, 1e-6);
    if (r > 1.0 + fw)
        discard;

    vec3 gray = vec3(0.45, 0.48, 0.52);
    vec3 rgb = vec3(0.0);
    float intensity = 0.0;
    for (int ring = 0; ring < 10; ++ring) {
        float radius = exp2(float(ring) - 9.0);
        float line = 1.0 - smoothstep(0.0, fw, abs(r - radius));
        intensity = max(intensity, line);
        rgb = mix(rgb, gray, line);
    }

    const float pi = 3.14159265;
    for (int ray = 0; ray < 16; ++ray) {
        float angle = float(ray) * (pi / 8.0);
        vec2 dir = vec2(cos(angle), sin(angle));
        float along = dot(p, dir);
        float start = 0.0;
        if (ray % 4 == 2)
            start = exp2(1.0 - 9.0);
        else if (ray % 4 != 0)
            start = exp2(3.0 - 9.0);
        if (along < start - fw || along > 1.0 + fw)
            continue;
        float dist = length(p - dir * along);
        float cover = 1.0 - smoothstep(start, start + fw, along);
        float line = (1.0 - cover) * (1.0 - smoothstep(0.0, fw, dist));
        vec3 tint = gray;
        if (ray == 0 || ray == 8)
            tint = vec3(0.50, 0.06, 0.06);
        else if (ray == 4 || ray == 12)
            tint = vec3(0.06, 0.06, 0.50);
        intensity = max(intensity, line);
        rgb = mix(rgb, tint, line);
    }

    float opacity = intensity * actorAlbedoOpacity.a;
    if (opacity < 0.01)
        discard;
    FragColor = vec4(rgb * opacity, opacity);
}
