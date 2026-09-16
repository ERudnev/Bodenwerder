#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in float v_geoBelow;
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
layout(binding = 7) uniform sampler2DArray u_farNormalMap;

const float shadowBias = 0.0005;
const float crustFreq = 0.1 / 28.0;
const float warpAmp = 0.28;
const float warpFreq = 12.0;
const float heightWarp = 0.04;
const float slope5 = 0.0038;
const float slope15 = 0.034;
const float gouraudBand = 0.1;
const float hillWidth = 12.0;
const float hillFadeStart = 500.0;
const float hillFadeEnd = 2000.0;
const float farSlopeGain = 2.2;
const float wrapAmount = 0.42;

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

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

vec4 valueNoiseGrad(vec3 x) {
    vec3 cell = floor(x);
    vec3 f = fract(x);
    vec3 u = f * f * (3.0 - 2.0 * f);
    vec3 du = 6.0 * f * (1.0 - f);
    float n000 = hash13(cell);
    float n100 = hash13(cell + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(cell + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(cell + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(cell + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(cell + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(cell + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(cell + vec3(1.0, 1.0, 1.0));
    float x00 = mix(n000, n100, u.x);
    float x10 = mix(n010, n110, u.x);
    float x01 = mix(n001, n101, u.x);
    float x11 = mix(n011, n111, u.x);
    float y0 = mix(x00, x10, u.y);
    float y1 = mix(x01, x11, u.y);
    float n = mix(y0, y1, u.z);
    float dnx = mix(mix(n100 - n000, n110 - n010, u.y), mix(n101 - n001, n111 - n011, u.y), u.z) * du.x;
    float dny = mix(x10 - x00, x11 - x01, u.z) * du.y;
    float dnz = (y1 - y0) * du.z;
    return vec4(n, dnx, dny, dnz);
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
    vec3 N = geoN;
    float below = v_geoBelow;
    float hillFade = 1.0 - smoothstep(hillFadeStart, hillFadeEnd, v_viewDistance);
    if (v_geometryStep <= 1 && hillFade > 0.0) {
        float gritA = sampleRoughness(layerOf(paletteA, 0u), axis);
        float gritB = sampleRoughness(layerOf(paletteB, 0u), axis);
        float gritC = sampleRoughness(layerOf(paletteC, 0u), axis);
        float grit = v_bary.x * gritA + v_bary.y * gritB + v_bary.z * gritC;
        vec3 hillUnit = v_objectPos * (1.0 / hillWidth);
        vec4 noise0 = valueNoiseGrad(hillUnit);
        vec4 noise1 = valueNoiseGrad(hillUnit * 6.283185 + vec3(19.0, 7.0, 13.0));
        float bump = max(noise0.x * 2.0 - 1.0, 0.0);
        float fine = max(noise1.x * 2.0 - 1.0, 0.0);
        float mound = bump + fine;
        vec3 gradObj = vec3(0.0);
        if (bump > 0.0)
            gradObj += 2.0 * noise0.yzw / hillWidth;
        if (fine > 0.0)
            gradObj += 2.0 * noise1.yzw * (6.283185 / hillWidth);
        vec3 gradH = mat3(actorModel) * (gradObj * 2.2);
        vec3 tilt = gradH - geoN * dot(gradH, geoN);
        vec3 bumpN = normalize(geoN - tilt);
        float hill = mound * grit * hillFade;
        N = normalize(mix(geoN, bumpN, min(hill, 1.0)));
        float hillTilt = 1.0 - clamp(dot(bumpN, geoN), 0.0, 1.0);
        below = clamp(v_geoBelow + smoothstep(slope5, slope15, hillTilt) * hill, 0.0, 1.0);
    }
    vec3 crustA = crustOf(paletteA, below, axis);
    vec3 crustB = crustOf(paletteB, below, axis);
    vec3 crustC = crustOf(paletteC, below, axis);
    float roughA = roughnessOf(paletteA, below, axis);
    float roughB = roughnessOf(paletteB, below, axis);
    float roughC = roughnessOf(paletteC, below, axis);
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
    vec3 nearAlbedo = nearWeights.x * crustA + nearWeights.y * crustB + nearWeights.z * crustC;
    vec3 gouraudAlbedo = v_bary.x * crustA + v_bary.y * crustB + v_bary.z * crustC;
    vec3 classicAlbedo = pow(max(gouraudAlbedo, vec3(1.0e-5)), vec3(0.8)) * pow(max(farSurface.rgb, vec3(1.0e-5)), vec3(0.2));
    vec3 albedo = mix(nearAlbedo, classicAlbedo, classicT);
    float nearRough = nearWeights.x * roughA + nearWeights.y * roughB + nearWeights.z * roughC;
    float gouraudRough = v_bary.x * roughA + v_bary.y * roughB + v_bary.z * roughC;
    float classicRough = pow(max(gouraudRough, 1.0e-5), 0.8) * pow(max(farSurface.a, 1.0e-5), 0.2);
    float roughness = mix(nearRough, classicRough, classicT);
    float farT = clamp(v_viewDistance / max(fieldLod.x * 0.8, 1.0), 0.0, 1.0);
    float farBlend = v_geometryStep > 1 ? 1.0 : pow(farT, 0.75);
    albedo = pow(max(albedo, vec3(1.0e-5)), vec3(1.0 - farBlend)) * pow(max(farSurface.rgb, vec3(1.0e-5)), vec3(farBlend));
    roughness = pow(max(roughness, 1.0e-5), 1.0 - farBlend) * pow(max(farSurface.a, 1.0e-5), farBlend);
    vec3 bakedObj = normalize(texture(u_farNormalMap, vec3(farUv, float(v_fieldDiamond))).xyz * 2.0 - 1.0);
    vec3 tilt = bakedObj - radial * dot(bakedObj, radial);
    vec3 steep = normalize(radial + tilt * farSlopeGain);
    N = normalize(mix(N, normalize(mat3(actorModel) * steep), farBlend));
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
    vec3 ambient = albedo * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * mix(1.0, 0.82, roughness);
    vec3 direct = albedo * lambert * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain)) * roughShade;
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
