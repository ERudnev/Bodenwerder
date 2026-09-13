#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
flat in vec4 v_color0;
flat in vec3 v_seed;
in vec3 v_bary;

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

layout(binding = 1) uniform sampler2D u_shadowMap;

const float shadowBias = 0.0005;
const float warpAmp = 0.28;
const float warpFreq = 12.0;
const float gouraudBand = 0.05;

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

float jagged(float t, float channel) {
    float n = 0.0;
    float amp = 1.0;
    for (int o = 0; o < 2; ++o) {
        float i = floor(t);
        float f = fract(t);
        float a = hash33(v_seed + vec3(i, channel, float(o))).x;
        float b = hash33(v_seed + vec3(i + 1.0, channel, float(o))).x;
        n += amp * mix(a, b, f);
        t = t * 2.27 + 3.1;
        amp *= 0.5;
    }
    return n * (2.0 / 1.5) - 1.0;
}

vec3 unpackRgb(float encoded) {
    uint bits = floatBitsToUint(encoded);
    return vec3(float(bits & 255u), float((bits >> 8) & 255u), float((bits >> 16) & 255u)) / 255.0;
}

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

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 c0 = unpackRgb(v_color0.x);
    vec3 c1 = unpackRgb(v_color0.y);
    vec3 c2 = unpackRgb(v_color0.z);
    vec3 gouraud = v_bary.x * c0 + v_bary.y * c1 + v_bary.z * c2;
    vec3 wave = vec3(jagged(dot(v_bary.yz, vec2(warpFreq)), 0.0), jagged(dot(v_bary.zx, vec2(warpFreq)), 1.0), jagged(dot(v_bary.xy, vec2(warpFreq)), 2.0));
    wave -= (wave.x + wave.y + wave.z) * (1.0 / 3.0);
    float interior = 27.0 * v_bary.x * v_bary.y * v_bary.z;
    vec3 warped = v_bary + wave * (warpAmp * interior);
    vec3 nearest = c2;
    if (warped.x > warped.y && warped.x > warped.z)
        nearest = c0;
    else if (warped.y > warped.z)
        nearest = c1;
    float top = max(warped.x, max(warped.y, warped.z));
    float low = min(warped.x, min(warped.y, warped.z));
    float gap = top - (warped.x + warped.y + warped.z - top - low);
    vec3 albedo = actorAlbedoOpacity.rgb * mix(nearest, gouraud, 1.0 - smoothstep(0.0, gouraudBand, gap));
    float lambert = max(dot(N, L), 0.0);
    float shadow = fetchShadow(v_worldPos, N, L);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = albedo * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain));
    vec3 direct = albedo * lambert * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain));
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
