#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in vec4 v_color0;
flat in uint v_palette;
in vec4 v_weights;

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
layout(binding = 1) uniform sampler2D u_shadowMap;

const float shadowBias = 0.0005;

float sampleShadow(vec2 uv, float currentDepth) {
    float closest = texture(u_shadowMap, uv).r;
    return currentDepth > closest ? 0.0 : 1.0;
}

float fetchShadow(vec3 worldPos, vec3 N, vec3 L) {
    float slope = 1.0 - max(dot(N, L), 0.0);
    vec4 lightSpace = passLightSpace * vec4(worldPos + N * (0.4 + 1.2 * slope), 1.0);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float currentDepth = proj.z - (shadowBias + 0.012 * slope);
    vec2 texel = 1.0 / vec2(textureSize(u_shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y)
            shadow += sampleShadow(proj.xy + vec2(float(x), float(y)) * texel, currentDepth);
    }
    return shadow / 9.0;
}

vec2 wrapGrad(vec2 d) {
    float mag2 = dot(d, d);
    return mag2 > 16.0 ? d * sqrt(16.0 / mag2) : d;
}

vec2 cubeFaceUv(vec3 dir) {
    vec3 extent = abs(dir);
    if (extent.x >= extent.y && extent.x >= extent.z)
        return vec2(dir.y, dir.z) / extent.x;
    if (extent.y >= extent.x && extent.y >= extent.z)
        return vec2(dir.x, dir.z) / extent.y;
    return vec2(dir.x, dir.y) / extent.z;
}

vec3 sampleLayer(vec2 uv, float layer, vec2 dx, vec2 dy) {
    return textureGrad(u_albedoMap, vec3(uv, layer), dx, dy).rgb;
}

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 dir = normalize(v_objectPos);
    float radius = actorLatticePattern.y * 0.5;
    vec2 uvRaw = cubeFaceUv(dir) * (radius / 40.0);
    vec2 dx = wrapGrad(dFdx(uvRaw));
    vec2 dy = wrapGrad(dFdy(uvRaw));
    vec2 uv = fract(uvRaw);
    vec4 w = v_weights;
    float mass = w.x + w.y + w.z + w.w;
    if (mass < 1.0e-5)
        w = vec4(1.0, 0.0, 0.0, 0.0);
    else
        w /= mass;
    vec3 albedo = w.x * sampleLayer(uv, float(v_palette & 15u), dx, dy);
    albedo += w.y * sampleLayer(uv, float((v_palette >> 4) & 15u), dx, dy);
    albedo += w.z * sampleLayer(uv, float((v_palette >> 8) & 15u), dx, dy);
    albedo += w.w * sampleLayer(uv, float((v_palette >> 12) & 15u), dx, dy);
    albedo *= actorAlbedoOpacity.rgb * v_color0.rgb;
    float lambert = max(dot(N, L), 0.0);
    float shadow = fetchShadow(v_worldPos, N, L);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = albedo * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain));
    vec3 direct = albedo * lambert * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain));
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
