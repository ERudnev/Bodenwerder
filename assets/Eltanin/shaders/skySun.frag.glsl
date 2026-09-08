#version 460 core

in vec3 v_dir;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
};

void main() {
    vec3 dir = normalize(v_dir);
    vec3 sunDir = normalize(passPrimaryLightPositionIntensity.xyz);
    float ang = acos(clamp(dot(dir, sunDir), -1.0, 1.0));
    float radius = radians(max(actorLatticePattern.y, 0.08)) * 0.5;
    float u = ang / max(radius, 1e-5);
    float disk = 1.0 - smoothstep(0.92, 1.02, u);
    float limb = sqrt(max(1.0 - u * u, 0.0));
    float corona = exp(-pow(max(u, 0.0) / 2.4, 1.65));
    vec3 color = passPrimaryLightColorRange.rgb;
    vec3 amber = color * vec3(1.0, 0.62, 0.28);
    vec3 tint = mix(amber, color, limb);
    float intensity = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 rgb = tint * intensity * (disk * (8.0 + 36.0 * limb) + corona * 0.08 * (1.0 - disk));
    if (dot(rgb, rgb) < 1e-4)
        discard;
    FragColor = vec4(rgb, 1.0);
    BloomMask = disk * (0.55 + 0.45 * limb);
}
