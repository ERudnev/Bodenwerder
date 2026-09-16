#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectNormal;
in vec3 v_objectPos;
flat in uvec3 v_layerPack;
flat in vec3 v_seed;
in vec3 v_bary;
in vec2 v_fieldUv;
flat in int v_fieldDiamond;
flat in int v_geometryStep;
in float v_viewDistance;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    float fieldRadius;
    float fieldAmplitude;
    int fieldSpan;
    int fieldCells;
    vec4 fieldLod;
    vec4 shell[12];
    ivec4 diamonds[10];
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
layout(binding = 6) uniform sampler2DArray u_farAlbedoMap;

const float shadowBias = 0.0005;
const float crustFreq = 1.0 / 28.0;
const float warpAmp = 0.28;
const float warpFreq = 12.0;
const float heightWarp = 0.04;
const float slope5 = 0.0038;
const float slope15 = 0.034;
const float gouraudBand = 0.1;

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

vec3 sampleCrust(float layer, vec3 axis) {
    vec3 alongX = texture(u_albedoMap, vec3(v_objectPos.yz * crustFreq, layer)).rgb;
    vec3 alongY = texture(u_albedoMap, vec3(v_objectPos.xz * crustFreq, layer)).rgb;
    vec3 alongZ = texture(u_albedoMap, vec3(v_objectPos.xy * crustFreq, layer)).rgb;
    return alongX * axis.x + alongY * axis.y + alongZ * axis.z;
}

float sampleRoughness(float layer, vec3 axis) {
    float alongX = texture(u_albedoMap, vec3(v_objectPos.yz * crustFreq, layer)).a;
    float alongY = texture(u_albedoMap, vec3(v_objectPos.xz * crustFreq, layer)).a;
    float alongZ = texture(u_albedoMap, vec3(v_objectPos.xy * crustFreq, layer)).a;
    return alongX * axis.x + alongY * axis.y + alongZ * axis.z;
}

float layerOf(uint palette, uint index) {
    return float((palette >> (index * 8u)) & 255u);
}

vec3 crustOf(uint palette, float below, vec3 axis) {
    return mix(sampleCrust(layerOf(palette, 0u), axis), sampleCrust(layerOf(palette, 1u), axis), below);
}

float roughnessOf(uint palette, float below, vec3 axis) {
    return mix(sampleRoughness(layerOf(palette, 0u), axis), sampleRoughness(layerOf(palette, 1u), axis), below);
}

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 radial = normalize(v_objectPos);
    vec3 axis = pow(abs(radial), vec3(4.0));
    axis /= max(axis.x + axis.y + axis.z, 1.0e-5);
    vec3 wave = vec3(jagged(dot(v_bary.yz, vec2(warpFreq)), 0.0), jagged(dot(v_bary.zx, vec2(warpFreq)), 1.0), jagged(dot(v_bary.xy, vec2(warpFreq)), 2.0));
    wave -= (wave.x + wave.y + wave.z) * (1.0 / 3.0);
    float interior = 27.0 * v_bary.x * v_bary.y * v_bary.z;
    float relief = (length(v_objectPos) - fieldRadius) / max(fieldAmplitude, 1.0);
    vec3 warped = v_bary + wave * (warpAmp * interior) + (v_bary - vec3(1.0 / 3.0)) * (relief * heightWarp);
    uint paletteA = v_layerPack.x;
    uint paletteB = v_layerPack.y;
    uint paletteC = v_layerPack.z;
    float slope = 1.0 - clamp(dot(v_objectNormal, radial), 0.0, 1.0);
    float below = smoothstep(slope5, slope15, slope);
    vec3 crustA = crustOf(paletteA, below, axis);
    vec3 crustB = crustOf(paletteB, below, axis);
    vec3 crustC = crustOf(paletteC, below, axis);
    float roughA = roughnessOf(paletteA, below, axis);
    float roughB = roughnessOf(paletteB, below, axis);
    float roughC = roughnessOf(paletteC, below, axis);
    vec3 nearest = crustC;
    float nearestRough = roughC;
    float lead = warped.z;
    float chase = max(warped.x, warped.y);
    if (warped.x >= warped.y && warped.x >= warped.z) {
        nearest = crustA;
        nearestRough = roughA;
        lead = warped.x;
        chase = max(warped.y, warped.z);
    } else if (warped.y >= warped.z) {
        nearest = crustB;
        nearestRough = roughB;
        lead = warped.y;
        chase = max(warped.x, warped.z);
    }
    vec3 gouraud = v_bary.x * crustA + v_bary.y * crustB + v_bary.z * crustC;
    float gouraudRough = v_bary.x * roughA + v_bary.y * roughB + v_bary.z * roughC;
    float rim = 1.0 - smoothstep(0.0, gouraudBand, lead - chase);
    vec3 albedo = mix(nearest, gouraud, rim);
    float roughness = mix(nearestRough, gouraudRough, rim);
    vec2 farTexel = 1.0 / vec2(textureSize(u_farAlbedoMap, 0).xy);
    vec2 farWarp = vec2(jagged(dot(v_objectPos.yz, vec2(0.04)), 3.0), jagged(dot(v_objectPos.xz, vec2(0.04)), 4.0));
    vec4 farSurface = texture(u_farAlbedoMap, vec3(clamp(v_fieldUv + farWarp * farTexel * 0.45, farTexel, 1.0 - farTexel), float(v_fieldDiamond)));
    float farT = clamp(v_viewDistance / max(fieldLod.x * 0.8, 1.0), 0.0, 1.0);
    float farBlend = v_geometryStep > 1 ? 1.0 : pow(farT, 0.75);
    albedo = pow(max(albedo, vec3(1.0e-5)), vec3(1.0 - farBlend)) * pow(max(farSurface.rgb, vec3(1.0e-5)), vec3(farBlend));
    roughness = pow(max(roughness, 1.0e-5), 1.0 - farBlend) * pow(max(farSurface.a, 1.0e-5), farBlend);
    albedo *= actorAlbedoOpacity.rgb;
    float lambert = max(dot(N, L), 0.0);
    float shadow = fetchShadow(v_worldPos, N, L);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = albedo * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * mix(1.0, 0.82, roughness);
    vec3 direct = albedo * lambert * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain)) * mix(1.0, 0.55, roughness);
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
