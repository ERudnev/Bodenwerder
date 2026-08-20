#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in vec4 v_mix0;
in vec4 v_mix1;
in vec4 v_mix2;
in vec4 v_mix3;
in vec2 v_heat;

out vec4 FragColor;

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

// Must match Mineral::table()
const float mineralScale[16] = float[](
    0.08, 0.25, 0.28, 0.22,
    0.45, 0.35, 0.55, 0.55,
    0.40, 0.38, 0.50, 0.70,
    0.42, 0.48, 0.20, 0.90
);
const float mineralRoughness[16] = float[](
    0.25, 0.72, 0.75, 0.68,
    0.88, 0.92, 0.32, 0.30,
    0.48, 0.62, 0.38, 0.22,
    0.55, 0.45, 0.40, 0.12
);
const float mineralMetalness[16] = float[](
    0.00, 0.00, 0.00, 0.00,
    0.00, 0.00, 1.00, 1.00,
    0.55, 0.20, 1.00, 1.00,
    0.35, 0.70, 0.00, 0.80
);
const float mineralMeltK[16] = float[](
    273.0, 2160.0, 1850.0, 1470.0,
    1780.0, 3900.0, 1811.0, 1728.0,
    1460.0, 1870.0, 1358.0, 2041.0,
    1290.0, 1405.0, 1074.0, 4200.0
);
const float mineralTintK[16] = float[](
    220.0, 850.0, 850.0, 780.0,
    700.0, 520.0, 800.0, 800.0,
    700.0, 850.0, 700.0, 900.0,
    750.0, 750.0, 600.0, 1600.0
);
const float mineralGlowK[16] = float[](
    900.0, 1400.0, 1350.0, 1200.0,
    1100.0, 900.0, 1200.0, 1180.0,
    1100.0, 1300.0, 1100.0, 1450.0,
    1150.0, 1150.0, 1000.0, 2600.0
);
const vec3 mineralSinter[16] = vec3[](
    vec3(0.220, 0.659, 1.000), vec3(0.165, 0.227, 0.098), vec3(0.141, 0.118, 0.098), vec3(0.541, 0.518, 0.486),
    vec3(0.384, 0.290, 0.188), vec3(0.063, 0.055, 0.047), vec3(0.769, 0.729, 0.690), vec3(0.824, 0.800, 0.729),
    vec3(0.659, 0.518, 0.227), vec3(0.251, 0.125, 0.086), vec3(0.910, 0.604, 0.306), vec3(0.769, 0.784, 0.824),
    vec3(0.290, 0.329, 0.275), vec3(0.204, 0.220, 0.157), vec3(0.973, 0.980, 0.988), vec3(0.659, 0.251, 1.000)
);

const float k_shadow_bias = 0.005;
const float pi = 3.14159265;
const float bumpHeightMeters = 0.05;
const float gritWeight = 0.30;
const float sinterStart = 0.8;

float sample_shadow(vec2 uv, float current_depth) {
    float closest = texture(u_shadowMap, uv).r;
    return current_depth > closest ? 0.0 : 1.0;
}

float fetch_shadow(vec4 light_space_pos, float slope) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }

    float current_depth = proj.z - (k_shadow_bias + 0.012 * slope);
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

float channelWeight(int channel) {
    vec4 groups[4] = vec4[4](v_mix0, v_mix1, v_mix2, v_mix3);
    return groups[channel / 4][channel - (channel / 4) * 4];
}

vec3 cameraWorldPos() {
    vec3 translation = passView[3].xyz;
    return -vec3(dot(passView[0].xyz, translation), dot(passView[1].xyz, translation), dot(passView[2].xyz, translation));
}

float crustLod(vec3 camera) {
    float dist = length(v_worldPos - camera);
    return clamp(log2(max(dist * 0.012, 1.0)), 0.0, 5.0);
}

vec4 sampleLayer(int channel, vec3 objectPos, float freqMul, float lod) {
    return textureLod(u_minerals[channel], fract(objectPos * mineralScale[channel] * freqMul), lod);
}

float crustHeight(vec3 objectPos, float lod, float gritAmt) {
    float height = 0.0;
    float mass = 0.0;
    for (int channel = 0; channel < 16; ++channel) {
        float weight = channelWeight(channel);
        if (weight <= 0.0)
            continue;
        vec4 crust = sampleLayer(channel, objectPos, 1.0, lod);
        vec4 grit = sampleLayer(channel, objectPos, 4.0, lod + 2.0);
        height += weight * (crust.a + gritWeight * gritAmt * (grit.a - 0.5));
        mass += weight;
    }
    if (mass <= 0.0)
        return 0.5;
    return clamp(height / mass, 0.0, 1.0);
}

vec3 perturbNormal(vec3 geometric, vec3 objectPos, float lod, float bumpAmt) {
    float amount = clamp(bumpAmt, 0.0, 1.0);
    if (amount <= 0.0)
        return geometric;
    float pixel = 0.5 * (length(dFdx(objectPos)) + length(dFdy(objectPos)));
    float step = max(0.06, pixel);
    float bumpLod = min(lod + log2(max(step / 0.06, 1.0)), 5.0);
    float fade = exp2(-0.85 * bumpLod);
    vec3 gradObject = vec3(
        crustHeight(objectPos + vec3(step, 0.0, 0.0), bumpLod, fade) - crustHeight(objectPos - vec3(step, 0.0, 0.0), bumpLod, fade),
        crustHeight(objectPos + vec3(0.0, step, 0.0), bumpLod, fade) - crustHeight(objectPos - vec3(0.0, step, 0.0), bumpLod, fade),
        crustHeight(objectPos + vec3(0.0, 0.0, step), bumpLod, fade) - crustHeight(objectPos - vec3(0.0, 0.0, step), bumpLod, fade)
    ) * (bumpHeightMeters / (2.0 * step));
    mat3 toWorld = mat3(transpose(inverse(actorModel)));
    vec3 gradWorld = toWorld * gradObject;
    vec3 tangentGrad = gradWorld - geometric * dot(gradWorld, geometric);
    vec3 bumped = normalize(geometric - amount * fade * tangentGrad);
    if (dot(bumped, geometric) < 0.0)
        return geometric;
    return bumped;
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

void applyHeat(int channel, float weight, float kelvin, inout vec3 albedo, inout float roughness, inout float metalness, inout vec3 emissive) {
    float tint = smoothstep(mineralTintK[channel], mineralGlowK[channel], kelvin);
    float glow = smoothstep(mineralGlowK[channel], mineralGlowK[channel] + 500.0, kelvin);
    float melt = smoothstep(mineralMeltK[channel] - 90.0, mineralMeltK[channel] + 70.0, kelvin);
    vec3 hot = blackbody(max(kelvin, mineralTintK[channel]));
    albedo = mix(albedo, mix(albedo, hot, 0.55), tint * weight);
    roughness = mix(roughness, mix(roughness, 0.10, melt), weight);
    metalness = mix(metalness, mix(metalness, 0.85, melt), weight);
    emissive += hot * glow * (2.4 + 5.5 * melt) * weight;
}

vec3 glazeAlbedo(vec3 albedo, vec3 sinterTint, float sinter) {
    float luma = max(dot(albedo, vec3(0.2126, 0.7152, 0.0722)), 0.001);
    vec3 glaze = sinterTint * mix(vec3(1.0), albedo / luma, 0.35);
    return mix(albedo, glaze, sinter);
}

vec3 glazeF0(vec3 albedo, vec3 sinterTint, float metalness, float sinter) {
    return mix(mix(vec3(0.04), sinterTint * 0.55, sinter), mix(albedo, sinterTint, sinter), metalness);
}

void main() {
    vec3 camera = cameraWorldPos();
    float lod = crustLod(camera);
    vec3 albedo = vec3(0.0);
    vec3 sinterTint = vec3(0.0);
    float height = 0.0;
    float roughness = 0.0;
    float metalness = 0.0;
    float mass = 0.0;
    for (int channel = 0; channel < 16; ++channel) {
        float weight = channelWeight(channel);
        if (weight <= 0.0)
            continue;
        vec4 crust = sampleLayer(channel, v_objectPos, 1.0, lod);
        vec4 grit = sampleLayer(channel, v_objectPos, 4.0, lod + 2.0);
        albedo += weight * crust.rgb;
        sinterTint += weight * mineralSinter[channel];
        height += weight * (crust.a + gritWeight * (grit.a - 0.5));
        roughness += weight * mineralRoughness[channel];
        metalness += weight * mineralMetalness[channel];
        mass += weight;
    }
    if (mass > 0.0) {
        albedo /= mass;
        sinterTint /= mass;
        height /= mass;
        roughness /= mass;
        metalness /= mass;
    }
    albedo *= actorAlbedoOpacity.rgb;
    height = clamp(height, 0.0, 1.0);
    float kelvin = max(v_heat.x, 0.0);
    float cohesion = clamp(v_heat.y, 0.0, 1.0);
    vec3 emissive = vec3(0.0);
    for (int channel = 0; channel < 16; ++channel) {
        float weight = channelWeight(channel);
        if (weight <= 0.0)
            continue;
        applyHeat(channel, weight, kelvin, albedo, roughness, metalness, emissive);
    }
    float bumpAmt = 1.0 - cohesion;
    float sinter = smoothstep(sinterStart, 1.0, cohesion);
    albedo = glazeAlbedo(albedo, sinterTint, sinter);
    roughness = mix(roughness, 0.08, sinter);
    roughness = clamp(roughness + 0.12 * (1.0 - height) * bumpAmt, 0.06, 1.0);

    vec3 geometric = normalize(v_worldNormal);
    vec3 N = perturbNormal(geometric, v_objectPos, lod, bumpAmt);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos);
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
    vec3 diffuse = kD * albedo;

    float cavity = mix(0.58, 1.0, height);
    float slope = 1.0 - max(dot(geometric, L), 0.0);
    float shadow = fetch_shadow(passLightSpace * vec4(v_worldPos, 1.0), slope);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);

    vec3 ambient = (kD * albedo + F0 * 0.22) * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * cavity;
    vec3 direct = (diffuse + specular) * passPrimaryLightColorRange.rgb * NdotL * shadow * (lightGain / (1.0 + lightGain)) * cavity;

    FragColor = vec4(ambient + direct + emissive, 1.0);
}
