#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in vec3 v_objectPos;
in vec2 v_atlas;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    float fieldRadius;
    float fieldAmplitude;
    int fieldSpan;
    int fieldCells;
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
layout(binding = 5) uniform sampler2D u_coverMap;

const float shadowBias = 0.0005;
const float crustFreq = 1.0 / 28.0;
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

vec4 faciesOf(vec2 cover, vec3 axis, float blend) {
    float surface = float(uint(cover.r * 255.0 + 0.5));
    float below = float(uint(cover.g * 255.0 + 0.5));
    return vec4(mix(sampleCrust(surface, axis), sampleCrust(below, axis), blend), mix(sampleRoughness(surface, axis), sampleRoughness(below, axis), blend));
}

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - v_worldPos * float(passPrimaryLightColorRange.w > 0.0));
    vec3 dir = normalize(v_objectPos);
    vec3 axis = pow(abs(dir), vec3(4.0));
    axis /= max(axis.x + axis.y + axis.z, 1.0e-5);
    float slope = 1.0 - clamp(dot(N, dir), 0.0, 1.0);
    float blend = smoothstep(slope5, slope15, slope);
    vec2 cellF = floor(v_atlas);
    vec2 frac = v_atlas - cellF;
    ivec2 cell = ivec2(cellF);
    ivec2 i0 = cell;
    ivec2 i1 = cell + ivec2(1, 0);
    ivec2 i2 = cell + ivec2(0, 1);
    vec3 bary = vec3(1.0 - frac.x - frac.y, frac.x, frac.y);
    if (frac.x + frac.y > 1.0) {
        i0 = cell + ivec2(1, 0);
        i1 = cell + ivec2(1, 1);
        i2 = cell + ivec2(0, 1);
        bary = vec3(1.0 - frac.y, frac.x + frac.y - 1.0, 1.0 - frac.x);
    }
    ivec2 last = textureSize(u_coverMap, 0) - 1;
    vec4 f0 = faciesOf(texelFetch(u_coverMap, clamp(i0, ivec2(0), last), 0).rg, axis, blend);
    vec4 f1 = faciesOf(texelFetch(u_coverMap, clamp(i1, ivec2(0), last), 0).rg, axis, blend);
    vec4 f2 = faciesOf(texelFetch(u_coverMap, clamp(i2, ivec2(0), last), 0).rg, axis, blend);
    vec4 gouraud = bary.x * f0 + bary.y * f1 + bary.z * f2;
    vec4 nearest = f2;
    float dominance = bary.z;
    float second = max(bary.x, bary.y);
    if (bary.x > bary.y && bary.x > bary.z) {
        nearest = f0;
        dominance = bary.x;
        second = max(bary.y, bary.z);
    } else if (bary.y > bary.z) {
        nearest = f1;
        dominance = bary.y;
        second = max(bary.x, bary.z);
    }
    vec4 crust = mix(nearest, gouraud, 1.0 - smoothstep(0.0, gouraudBand, dominance - second));
    vec3 albedo = crust.rgb * actorAlbedoOpacity.rgb;
    float roughness = crust.a;
    float lambert = max(dot(N, L), 0.0);
    float shadow = fetchShadow(v_worldPos, N, L);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = albedo * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * mix(1.0, 0.82, roughness);
    vec3 direct = albedo * lambert * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain)) * mix(1.0, 0.55, roughness);
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
