#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec2 v_uv0;
flat in uint v_drawId;

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

void main() {
    uvec2 drawMetadata = metadata[v_drawId];
    uint surface = primitiveSurfaces[drawMetadata.x + uint(gl_PrimitiveID)];
    uint layer = surfacePalette[drawMetadata.y + surface];
    vec4 texel = texture(u_albedoMap, vec3(v_uv0, float(layer)));
    if (texel.a < 0.01) {
        discard;
    }

    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 baseColor = texel.rgb * actorAlbedoOpacity.rgb;

    float ndotl = max(dot(N, L), 0.0);
    // Low-alpha fringe: fade shadow out so soft edges don't darken the background via blend.
    float shadow = mix(
        1.0,
        fetch_shadow(passLightSpace * vec4(v_worldPos, 1.0)),
        smoothstep(0.25, 0.75, texel.a));

    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);

    vec3 ambient = baseColor * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain));
    vec3 direct = baseColor * passPrimaryLightColorRange.rgb * ndotl * shadow * (lightGain / (1.0 + lightGain));

    FragColor = vec4(ambient + direct, texel.a * actorAlbedoOpacity.a);
}
