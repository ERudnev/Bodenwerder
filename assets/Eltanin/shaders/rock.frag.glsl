#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
flat in uvec2 v_mix0;

out vec4 FragColor;

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

layout(binding = 1) uniform sampler2D u_shadowMap;
layout(binding = 3) uniform sampler3D u_minerals[16];

// Must match Mineral::table() .scale
const float mineralScale[16] = float[](
    0.08, 0.25, 0.28, 0.22,
    0.45, 0.35, 0.55, 0.55,
    0.40, 0.38, 0.50, 0.70,
    0.42, 0.48, 0.20, 0.90
);

const float k_shadow_bias = 0.005;

float sample_shadow(vec2 uv, float current_depth) {
    float closest = texture(u_shadowMap, uv).r;
    return current_depth > closest ? 0.0 : 1.0;
}

float fetch_shadow(vec4 light_space_pos) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }

    float current_depth = proj.z - k_shadow_bias;
    vec2 texel = 1.0 / vec2(textureSize(u_shadowMap, 0));

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            shadow += sample_shadow(proj.xy + offset, current_depth);
        }
    }

    return shadow / 9.0;
}

float nibbleWeight(uint nibbleIndex) {
    uint word = nibbleIndex < 8u ? v_mix0.x : v_mix0.y;
    uint shift = (nibbleIndex & 7u) * 4u;
    return float((word >> shift) & 15u) / 15.0;
}

void main() {
    vec3 albedo = vec3(0.0);
    float mass = 0.0;
    for (int channel = 0; channel < 16; ++channel) {
        float weight = nibbleWeight(uint(channel));
        if (weight <= 0.0)
            continue;
        vec3 uvw = fract(v_objectPos * mineralScale[channel]);
        albedo += weight * texture(u_minerals[channel], uvw).rgb;
        mass += weight;
    }
    if (mass > 0.0)
        albedo /= mass;
    albedo *= actorAlbedoOpacity.rgb;

    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos);

    float ndotl = max(dot(N, L), 0.0);
    float shadow = fetch_shadow(passLightSpace * vec4(v_worldPos, 1.0));

    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);

    vec3 ambient = albedo * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain));
    vec3 direct = albedo * passPrimaryLightColorRange.rgb * ndotl * shadow * (lightGain / (1.0 + lightGain));

    FragColor = vec4(ambient + direct, 1.0);
}
