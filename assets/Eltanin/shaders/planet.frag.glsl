#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in float v_geoBelow;
flat in vec3 v_belowBlotch;
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
layout(binding = 4) uniform sampler2DArray u_albedoLow;
layout(binding = 6) uniform sampler2DArray u_farAlbedoMap;
layout(binding = 7) uniform sampler2DArray u_farNormalMap;

const float shadowBias = 0.0005;
const float crustFreq = 0.1 / 28.0;
const float faciesLowMul = 19.0;
const float pairNear = 12.0;
const float pairFar = 280.0;
const float pairClose = 0.85; // leftover landscape share kills close tiling
const float slopeDead = 0.00061; // ~2°, flats stay clean
const float slopeTail = 0.0075; // ~7°, light-slope tail peaks
const float slope5 = 0.0038;
const float slope45 = 0.293;
const float belowBlotchAmt = 0.55;
const float belowFlatTail = 0.20;
const float warpAmp = 0.28;
const float warpFreq = 12.0;
const float heightWarp = 0.04;
const float gouraudBand = 0.1;
const float farSlopeGain = 2.2;
const float wrapAmount = 0.42;

float sampleShadow(vec2 uv, float currentDepth) {
    float closest = texture(u_shadowMap, uv).r;
    return currentDepth < closest ? 0.0 : 1.0;
}

float fetchShadow(vec3 worldPos, vec3 N, vec3 L) {
    float slope = 1.0 - max(dot(N, L), 0.0);
    vec4 lightSpace = passLightSpace * vec4(worldPos + N * (0.4 + 1.2 * slope), 1.0);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    vec2 uv = proj.xy * 0.5 + 0.5;
    if (proj.z < 0.0 || proj.z > 1.0 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    float currentDepth = proj.z + (shadowBias + 0.012 * slope);
    vec2 texel = 1.0 / vec2(textureSize(u_shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y)
            shadow += sampleShadow(uv + vec2(float(x), float(y)) * texel, currentDepth);
    }
    return shadow / 9.0;
}

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

float jagged(float t, float channel, vec3 seed) {
    float n = 0.0;
    float amp = 1.0;
    for (int o = 0; o < 2; ++o) {
        float i = floor(t);
        float f = fract(t);
        float a = hash33(seed + vec3(i, channel, float(o))).x;
        float b = hash33(seed + vec3(i + 1.0, channel, float(o))).x;
        n += amp * mix(a, b, f);
        t = t * 2.27 + 3.1;
        amp *= 0.5;
    }
    return n * (2.0 / 1.5) - 1.0;
}

vec4 sampleTriplanar(sampler2DArray map, float freq, float layer, vec3 axis) {
    vec4 alongX = texture(map, vec3(v_objectPos.yz * freq, layer));
    vec4 alongY = texture(map, vec3(v_objectPos.xz * freq, layer));
    vec4 alongZ = texture(map, vec3(v_objectPos.xy * freq, layer));
    return alongX * axis.x + alongY * axis.y + alongZ * axis.z;
}

vec4 samplePair(float layer, vec3 axis, float lowW) {
    vec4 high = sampleTriplanar(u_albedoMap, crustFreq, layer, axis);
    if (lowW <= 0.02)
        return high;
    vec4 low = sampleTriplanar(u_albedoLow, crustFreq * faciesLowMul, layer, axis);
    return mix(high, low, lowW * pairClose);
}

float layerOf(uint palette, uint index) {
    return float((palette >> (index * 8u)) & 255u);
}

vec4 crustOf(uint palette, float below, vec3 axis, float lowW) {
    return mix(samplePair(layerOf(palette, 0u), axis, lowW), samplePair(layerOf(palette, 1u), axis, lowW), below);
}

float belowOf(float h, float grade, float tailGate) {
    float thresh = mix(0.82, 0.45, tailGate);
    return mix(smoothstep(thresh, thresh + 0.14, h) * belowFlatTail * tailGate, 1.0 - smoothstep(0.80, 0.94, h) * belowBlotchAmt, grade);
}

void main() {
    vec3 geoN = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 radial = normalize(v_objectPos);
    vec3 axis = pow(abs(radial), vec3(4.0));
    axis /= max(axis.x + axis.y + axis.z, 1.0e-5);
    vec3 wave = vec3(jagged(dot(v_bary.yz, vec2(warpFreq)), 0.0, v_seed), jagged(dot(v_bary.zx, vec2(warpFreq)), 1.0, v_seed), jagged(dot(v_bary.xy, vec2(warpFreq)), 2.0, v_seed));
    wave -= (wave.x + wave.y + wave.z) * (1.0 / 3.0);
    float interior = 27.0 * v_bary.x * v_bary.y * v_bary.z;
    float relief = (length(v_objectPos) - fieldRadius) / max(fieldAmplitude, 1.0);
    vec3 warped = v_bary + wave * (warpAmp * interior) + (v_bary - vec3(1.0 / 3.0)) * (relief * heightWarp);
    uint paletteA = v_layerPack.x;
    uint paletteB = v_layerPack.y;
    uint paletteC = v_layerPack.z;
    float lowW = 1.0 - smoothstep(pairNear, pairFar, v_viewDistance);
    float grade = smoothstep(slope5, slope45, v_geoBelow);
    float tailGate = smoothstep(slopeDead, slopeTail, v_geoBelow);
    vec4 crustA = crustOf(paletteA, belowOf(v_belowBlotch.x, grade, tailGate), axis, lowW);
    vec4 crustB = crustOf(paletteB, belowOf(v_belowBlotch.y, grade, tailGate), axis, lowW);
    vec4 crustC = crustOf(paletteC, belowOf(v_belowBlotch.z, grade, tailGate), axis, lowW);
    vec3 voronoi = vec3(0.0, 0.0, 1.0);
    float lead = warped.z;
    float chase = max(warped.x, warped.y);
    if (warped.x >= warped.y && warped.x >= warped.z) {
        voronoi = vec3(1.0, 0.0, 0.0);
        lead = warped.x;
        chase = max(warped.y, warped.z);
    } else if (warped.y >= warped.z) {
        voronoi = vec3(0.0, 1.0, 0.0);
        lead = warped.y;
        chase = max(warped.x, warped.z);
    }
    float rim = 1.0 - smoothstep(0.0, gouraudBand, lead - chase);
    vec3 nearWeights = mix(voronoi, v_bary, rim);
    float classicT = clamp((v_viewDistance - 100.0) / 1900.0, 0.0, 1.0);
    vec2 farTexel = 1.0 / vec2(textureSize(u_farAlbedoMap, 0).xy);
    vec2 farUv = clamp(v_fieldUv + vec2(jagged(dot(v_objectPos.yz, vec2(0.04)), 3.0, v_seed), jagged(dot(v_objectPos.xz, vec2(0.04)), 4.0, v_seed)) * farTexel * 0.45, farTexel, 1.0 - farTexel);
    vec4 farSurface = texture(u_farAlbedoMap, vec3(farUv, float(v_fieldDiamond)));
    vec3 nearAlbedo = nearWeights.x * crustA.rgb + nearWeights.y * crustB.rgb + nearWeights.z * crustC.rgb;
    vec3 gouraudAlbedo = v_bary.x * crustA.rgb + v_bary.y * crustB.rgb + v_bary.z * crustC.rgb;
    vec3 classicAlbedo = pow(max(gouraudAlbedo, vec3(1.0e-5)), vec3(0.8)) * pow(max(farSurface.rgb, vec3(1.0e-5)), vec3(0.2));
    vec3 albedo = mix(nearAlbedo, classicAlbedo, classicT);
    float nearRough = nearWeights.x * crustA.a + nearWeights.y * crustB.a + nearWeights.z * crustC.a;
    float gouraudRough = v_bary.x * crustA.a + v_bary.y * crustB.a + v_bary.z * crustC.a;
    float classicRough = pow(max(gouraudRough, 1.0e-5), 0.8) * pow(max(farSurface.a, 1.0e-5), 0.2);
    float roughness = mix(nearRough, classicRough, classicT);
    float farT = clamp(v_viewDistance / max(fieldLod.x * 0.8, 1.0), 0.0, 1.0);
    float farBlend = v_geometryStep > 1 ? 1.0 : pow(farT, 0.75);
    albedo = pow(max(albedo, vec3(1.0e-5)), vec3(1.0 - farBlend)) * pow(max(farSurface.rgb, vec3(1.0e-5)), vec3(farBlend));
    roughness = pow(max(roughness, 1.0e-5), 1.0 - farBlend) * pow(max(farSurface.a, 1.0e-5), farBlend);
    vec3 bakedObj = normalize(texture(u_farNormalMap, vec3(farUv, float(v_fieldDiamond))).xyz * 2.0 - 1.0);
    vec3 tilt = bakedObj - radial * dot(bakedObj, radial);
    vec3 steep = normalize(radial + tilt * farSlopeGain);
    vec3 N = normalize(mix(geoN, normalize(mat3(actorModel) * steep), farBlend));
    albedo *= actorAlbedoOpacity.rgb;
    float ndl = dot(N, L);
    float hard = max(ndl, 0.0);
    float wrapped = clamp((ndl + wrapAmount) / (1.0 + wrapAmount), 0.0, 1.0);
    wrapped *= wrapped;
    float lambert = mix(hard, wrapped, mix(0.2, 0.82, farBlend));
    float shadow = fetchShadow(v_worldPos, N, L);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    float roughShade = mix(mix(1.0, 0.55, roughness), mix(1.0, 0.88, roughness), farBlend);
    float fill = ambientGain / (1.0 + ambientGain);
    vec3 bounce = mix(vec3(0.18), albedo, 0.1827);
    vec3 ambient = bounce * passAmbientColorIntensity.rgb * fill * mix(1.0, 0.82, roughness);
    vec3 direct = albedo * lambert * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain)) * roughShade;
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
