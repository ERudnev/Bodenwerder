#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec2 v_uv0;
flat in uint v_drawId;
flat in float v_cohesion;
flat in float v_heat;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
    vec4 actorHeat;
};

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
};

layout(binding = 0) uniform sampler2DArray u_albedoMap;

layout(std430, binding = 9) readonly buffer DrawMetadataBuffer {
    uvec2 metadata[];
};
layout(std430, binding = 10) readonly buffer SurfacePaletteBuffer {
    uint surfacePalette[];
};
layout(std430, binding = 11) readonly buffer PrimitiveSurfacesBuffer {
    uint primitiveSurfaces[];
};

layout(binding = 1) uniform sampler2D u_shadowMap;

const float k_shadow_bias = 0.001;
const float k_shadow_slope = 0.01;

float sample_shadow(vec2 uv, float current_depth) {
    float closest = texture(u_shadowMap, uv).r;
    return current_depth > closest ? 0.0 : 1.0;
}

float fetch_shadow(vec3 worldPos, vec3 N, vec3 L) {
    float slope = 1.0 - max(dot(N, L), 0.0);
    vec4 light_space_pos = passLightSpace * vec4(worldPos + N * (0.4 + 1.2 * slope), 1.0);
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }

    float current_depth = proj.z - (k_shadow_bias + k_shadow_slope * slope);
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

vec3 blackbody(float kelvin) {
    float t = clamp((kelvin - 700.0) / 2500.0, 0.0, 1.0);
    vec3 ember = vec3(0.55, 0.04, 0.01);
    vec3 red = vec3(1.0, 0.14, 0.02);
    vec3 orange = vec3(1.0, 0.42, 0.06);
    vec3 yellow = vec3(1.0, 0.82, 0.38);
    vec3 white = vec3(1.0, 0.96, 0.90);
    if (t < 0.22)
        return mix(ember, red, t / 0.22);
    if (t < 0.45)
        return mix(red, orange, (t - 0.22) / 0.23);
    if (t < 0.72)
        return mix(orange, yellow, (t - 0.45) / 0.27);
    return mix(yellow, white, (t - 0.72) / 0.28);
}

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    uvec2 drawMetadata = metadata[v_drawId];
    uint surface = primitiveSurfaces[drawMetadata.x + uint(gl_PrimitiveID)];
    uint layer = surfacePalette[drawMetadata.y + surface];
    vec3 intact = texture(u_albedoMap, vec3(v_uv0, float(layer))).rgb;
    vec3 baseColor = intact * actorAlbedoOpacity.rgb;
    if (actorHeat.z >= 0.0 && actorHeat.w >= 0.0) {
        float mask = texture(u_albedoMap, vec3(v_uv0, actorHeat.w)).r;
        vec3 wrecked = texture(u_albedoMap, vec3(v_uv0, actorHeat.z)).rgb;
        if (mask >= v_cohesion)
            baseColor = wrecked * actorAlbedoOpacity.rgb;
    }

    float kelvin = max(v_heat, 0.0);
    float tint = smoothstep(800.0, 1400.0, kelvin);
    float glow = smoothstep(900.0, 1600.0, kelvin);
    float melt = smoothstep(1700.0, 2100.0, kelvin);
    vec3 hot = blackbody(max(kelvin, 800.0));
    baseColor = mix(baseColor, mix(baseColor, hot, 0.55), tint);

    float ndotl = max(dot(N, L), 0.0);
    float shadow = fetch_shadow(v_worldPos, N, L);

    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);

    vec3 ambient = baseColor * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain));
    vec3 direct = baseColor * passPrimaryLightColorRange.rgb * ndotl * shadow * (lightGain / (1.0 + lightGain));
    vec3 emissive = hot * glow * (2.4 + 5.5 * melt);

    FragColor = vec4(ambient + direct + emissive, 1.0);
    BloomMask = glow * (0.35 + 0.65 * melt);
}
