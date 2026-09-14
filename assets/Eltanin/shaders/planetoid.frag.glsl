#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in vec3 v_objectNormal;
in vec2 v_drivers;
in float v_cohesion;

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
layout(binding = 0) uniform sampler2DArray u_albedoMap;

const float roughIce = 0.25;
const float roughOlivine = 0.72;
const float roughPyroxene = 0.75;
const float roughIron = 0.40;
const float metalIce = 0.00;
const float metalOlivine = 0.00;
const float metalPyroxene = 0.00;
const float metalIron = 0.55;
const vec3 sinterIce = vec3(0.220, 0.659, 1.000);
const vec3 sinterOlivine = vec3(0.196, 0.212, 0.141);
const vec3 sinterPyroxene = vec3(0.141, 0.118, 0.098);
const vec3 sinterIron = vec3(1.000, 0.659, 0.251);

const float k_shadow_bias = 0.0005;
const float pi = 3.14159265;
const float sinterStart = 0.45;

float sample_shadow(vec2 uv, float current_depth) {
    float closest = texture(u_shadowMap, uv).r;
    return current_depth > closest ? 0.0 : 1.0;
}

float fetch_shadow(vec4 light_space_pos, float slope, float filterAmt) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float current_depth = proj.z - (k_shadow_bias + 0.012 * slope);
    if (filterAmt <= 0.02)
        return sample_shadow(proj.xy, current_depth);
    vec2 texel = 1.0 / vec2(textureSize(u_shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y)
            shadow += sample_shadow(proj.xy + vec2(float(x), float(y)) * texel, current_depth);
    }
    return shadow / 9.0;
}

vec3 cameraWorldPos() {
    vec3 translation = passView[3].xyz;
    return -vec3(dot(passView[0].xyz, translation), dot(passView[1].xyz, translation), dot(passView[2].xyz, translation));
}

vec2 wrapGrad(vec2 d) {
    float m2 = dot(d, d);
    return m2 > 16.0 ? d * sqrt(16.0 / m2) : d;
}

vec3 sampleAlbedo(sampler2DArray pack, vec2 uv, float layer, vec2 dx, vec2 dy) {
    return textureGrad(pack, vec3(uv, layer), dx, dy).rgb;
}

vec2 cubeFaceUv(vec3 dir) {
    vec3 extent = abs(dir);
    if (extent.x >= extent.y && extent.x >= extent.z)
        return vec2(dir.y, dir.z) / extent.x;
    if (extent.y >= extent.x && extent.y >= extent.z)
        return vec2(dir.x, dir.z) / extent.y;
    return vec2(dir.x, dir.y) / extent.z;
}

float distributionGgx(float NdotH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (NdotH * NdotH) * (alpha2 - 1.0) + 1.0;
    return alpha2 / (pi * denom * denom);
}

float geometrySchlick(float NdotX, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 glazeAlbedo(vec3 albedo, vec3 sinterTint, float sinter) {
    float luma = max(dot(albedo, vec3(0.2126, 0.7152, 0.0722)), 0.001);
    vec3 glaze = sinterTint * mix(vec3(1.0), albedo / luma, 0.18);
    return mix(albedo, glaze, sinter);
}

vec3 glazeF0(vec3 albedo, vec3 sinterTint, float metalness, float sinter) {
    return mix(mix(vec3(0.04), sinterTint * 0.55, sinter), mix(albedo, sinterTint, sinter), metalness);
}

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));
    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    return mix(mix(nx00, nx10, f.y), mix(nx01, nx11, f.y), f.z);
}

float surfaceMottle(vec3 pos) {
    float coarse = valueNoise(pos * (1.0 / 80.0));
    float mid = valueNoise(pos * (1.0 / 28.0) + 19.0);
    float fine = valueNoise(pos * (1.0 / 28.0) * 5.3 + 41.0);
    return coarse * 0.50 + mid * 0.31 + fine * 0.19;
}

void main() {
    vec3 camera = cameraWorldPos();
    vec3 dir = normalize(v_objectPos);
    float altitude = v_drivers.x;
    float crater = v_drivers.y;
    float slope = 1.0 - clamp(dot(normalize(v_objectNormal), dir), 0.0, 1.0);
    float polar = abs(dir.y);
    float bowl = smoothstep(0.05, 0.85, clamp(-crater, 0.0, 1.4) / 1.4);
    float flats = 1.0 - slope;
    float midLat = 1.0 - smoothstep(0.45, 0.82, polar);
    float highland = smoothstep(0.05, 0.55, altitude);

    float wIce = 0.85 * smoothstep(0.52, 0.88, polar);
    float wOlivine = 0.55 * flats * midLat * (0.55 + 0.45 * (1.0 - highland));
    float wPyroxene = 0.48 * (0.35 + 0.65 * highland) * (0.55 + 0.45 * midLat);
    float wIron = 0.70 * bowl * (0.45 + 0.55 * midLat);
    float mass = wIce + wOlivine + wPyroxene + wIron;
    if (mass < 1.0e-5) {
        wOlivine = 1.0;
        mass = 1.0;
    }
    wIce /= mass;
    wOlivine /= mass;
    wPyroxene /= mass;
    wIron /= mass;

    float radius = actorLatticePattern.y * 0.5;
    vec2 faceUv = cubeFaceUv(dir);
    vec2 uvCloseRaw = faceUv * (radius / 40.0);
    vec2 closeDx = wrapGrad(dFdx(uvCloseRaw));
    vec2 closeDy = wrapGrad(dFdy(uvCloseRaw));
    vec2 uvClose = fract(uvCloseRaw);
    float pixelMeters = 0.5 * (length(dFdx(v_worldPos)) + length(dFdy(v_worldPos)));
    float closeAmt = 1.0 - smoothstep(1.2, 5.0, pixelMeters);

    vec3 aIce = sampleAlbedo(u_albedoMap, uvClose, 0.0, closeDx, closeDy);
    vec3 aOlivine = sampleAlbedo(u_albedoMap, uvClose, 5.0, closeDx, closeDy);
    vec3 aPyroxene = sampleAlbedo(u_albedoMap, uvClose, 7.0, closeDx, closeDy);
    vec3 aIron = sampleAlbedo(u_albedoMap, uvClose, 24.0, closeDx, closeDy);
    vec3 albedo = wIce * aIce + wOlivine * aOlivine + wPyroxene * aPyroxene + wIron * aIron;
    albedo *= mix(0.74, 1.18, surfaceMottle(v_objectPos));

    vec3 sinterTint = wIce * sinterIce + wOlivine * sinterOlivine + wPyroxene * sinterPyroxene + wIron * sinterIron;
    float roughness = wIce * roughIce + wOlivine * roughOlivine + wPyroxene * roughPyroxene + wIron * roughIron;
    float metalness = wIce * metalIce + wOlivine * metalOlivine + wPyroxene * metalPyroxene + wIron * metalIron;

    albedo *= actorAlbedoOpacity.rgb;
    float cohesion = clamp(v_cohesion, 0.0, 1.0);
    float looseness = 1.0 - cohesion;
    float sinter = smoothstep(sinterStart, 1.0, cohesion);
    albedo = glazeAlbedo(albedo, sinterTint, sinter);
    albedo *= mix(1.0, 0.82, looseness);
    roughness = mix(roughness, 0.08, sinter);
    roughness = clamp(roughness + 0.22 * looseness, 0.06, 1.0);
    metalness *= mix(1.0, 0.12, looseness);

    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 V = normalize(camera - v_worldPos);
    vec3 H = normalize(V + L);
    float nDotL = max(dot(N, L), 0.0);
    float fade = sin(radians(15.0));
    float mu0 = nDotL * smoothstep(0.0, fade, nDotL);
    float mu = max(dot(N, V), 0.0);
    float NdotL = mu0;
    float NdotV = max(mu, 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float lunarK = 0.90 * looseness * looseness;
    float lunarLambert = (1.0 - lunarK) * mu0 + lunarK * (2.0 * mu0 / (mu0 + mu + 1.0e-4));
    vec3 F0 = glazeF0(albedo, sinterTint, metalness, sinter);
    vec3 F = fresnelSchlick(VdotH, F0);
    float D = distributionGgx(NdotH, roughness);
    float G = geometrySchlick(NdotV, roughness) * geometrySchlick(NdotL, roughness);
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 0.001);
    specular *= mix(1.0, 0.05, looseness);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);
    float cavity = mix(1.0, 0.92, sinter);
    float lightSlope = 1.0 - mu0;
    float shadow = fetch_shadow(passLightSpace * vec4(v_worldPos + N * (0.4 + 1.2 * lightSlope), 1.0), lightSlope, closeAmt);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = (kD * albedo + F0 * 0.22) * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * cavity;
    vec3 direct = (kD * albedo * lunarLambert + specular) * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain)) * cavity;
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
