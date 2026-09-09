#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in vec3 v_objectNormal;
in vec2 v_drivers;

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
layout(binding = 3) uniform sampler3D u_minerals[16];

// Fixed planetoid demo palette: Ice, Olivine, Pyroxene, Iron (Mineral::table indices).
const float scaleIce = 0.08;
const float scaleOlivine = 0.25;
const float scalePyroxene = 0.28;
const float scaleIron = 0.32;
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

float fetch_shadow(vec4 light_space_pos, float slope) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float current_depth = proj.z - (k_shadow_bias + 0.012 * slope);
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

float crustLod(vec3 camera) {
    return clamp(log2(max(length(v_worldPos - camera) * 0.012, 1.0)), 0.0, 5.0);
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

void main() {
    vec3 camera = cameraWorldPos();
    float lod = crustLod(camera);
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

    vec4 cIce = textureLod(u_minerals[0], fract(v_objectPos * scaleIce), lod);
    vec4 cOlivine = textureLod(u_minerals[1], fract(v_objectPos * scaleOlivine), lod);
    vec4 cPyroxene = textureLod(u_minerals[2], fract(v_objectPos * scalePyroxene), lod);
    vec4 cIron = textureLod(u_minerals[6], fract(v_objectPos * scaleIron), lod);

    vec3 albedo = wIce * cIce.rgb + wOlivine * cOlivine.rgb + wPyroxene * cPyroxene.rgb + wIron * cIron.rgb;
    vec3 sinterTint = wIce * sinterIce + wOlivine * sinterOlivine + wPyroxene * sinterPyroxene + wIron * sinterIron;
    float height = wIce * cIce.a + wOlivine * cOlivine.a + wPyroxene * cPyroxene.a + wIron * cIron.a;
    float roughness = wIce * roughIce + wOlivine * roughOlivine + wPyroxene * roughPyroxene + wIron * roughIron;
    float metalness = wIce * metalIce + wOlivine * metalOlivine + wPyroxene * metalPyroxene + wIron * metalIron;

    albedo *= actorAlbedoOpacity.rgb;
    height = clamp(height, 0.0, 1.0);
    float cohesion = clamp(0.08 + 0.35 * highland * flats + 0.40 * bowl + 0.25 * wIce, 0.0, 0.7);
    float sinter = smoothstep(sinterStart, 1.0, cohesion);
    albedo = glazeAlbedo(albedo, sinterTint, sinter);
    roughness = mix(roughness, 0.08, sinter);
    roughness = clamp(roughness + 0.12 * (1.0 - height) * (1.0 - cohesion), 0.06, 1.0);

    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 V = normalize(camera - v_worldPos);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    vec3 F0 = glazeF0(albedo, sinterTint, metalness, sinter);
    vec3 F = fresnelSchlick(VdotH, F0);
    float D = distributionGgx(NdotH, roughness);
    float G = geometrySchlick(NdotV, roughness) * geometrySchlick(NdotL, roughness);
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);
    float cavity = mix(mix(0.58, 1.0, height), mix(0.78, 1.0, height), sinter);
    float lightSlope = 1.0 - max(dot(N, L), 0.0);
    float shadow = fetch_shadow(passLightSpace * vec4(v_worldPos + N * (0.4 + 1.2 * lightSlope), 1.0), lightSlope);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = (kD * albedo + F0 * 0.22) * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * cavity;
    vec3 direct = (kD * albedo + specular) * passPrimaryLightColorRange.rgb * NdotL * shadow * (lightGain / (1.0 + lightGain)) * cavity;
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
