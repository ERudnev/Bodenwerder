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
layout(binding = 2) uniform sampler2D u_nearShadowMap;
layout(std140, binding = 1) uniform NearShadowState {
    mat4 nearLightSpace;
    vec4 nearCameraRange;
};
layout(binding = 19) uniform sampler2D u_mediumShadowMap;
layout(std140, binding = 2) uniform MediumShadowState {
    mat4 mediumLightSpace;
    vec4 mediumCameraRange;
};
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

const float pi = 3.14159265;
// Compacted soil is not automatically a dark, glossy fused surface.
const float sinterStart = 0.88;

float sample_shadow(sampler2D map, vec2 uv, float current_depth) {
    float closest = texture(map, uv).r;
    return current_depth > closest ? 0.0 : 1.0;
}

float fetch_shadow(sampler2D map, mat4 lightSpace, vec3 normal, float slope) {
    // Bias follows a shadow texel in world units, not a fraction of the whole
    // planet's depth range (the former 0.012 slope term could erase 100s of m).
    vec2 mapSize = vec2(textureSize(map, 0));
    vec3 lightX = vec3(lightSpace[0].x, lightSpace[1].x, lightSpace[2].x);
    vec3 lightY = vec3(lightSpace[0].y, lightSpace[1].y, lightSpace[2].y);
    vec3 lightZ = vec3(lightSpace[0].z, lightSpace[1].z, lightSpace[2].z);
    float texelMeters = max(2.0 / (mapSize.x * length(lightX)), 2.0 / (mapSize.y * length(lightY)));
    vec4 light_space_pos = lightSpace * vec4(v_worldPos + normal * texelMeters * (0.5 + slope), 1.0);
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z < 0.0 || proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float current_depth = proj.z - max(0.0000002, texelMeters * length(lightZ) * 0.05);
    // Continuous 3x3 comparison filtering, also at orbital distance. Weights
    // follow the sub-texel position so the edge does not jump between cells.
    vec2 phase = fract(proj.xy * mapSize) - 0.5;
    vec2 lower = 0.5 * (0.5 - phase) * (0.5 - phase);
    vec2 upper = 0.5 * (0.5 + phase) * (0.5 + phase);
    vec3 weightsX = vec3(lower.x, 0.75 - phase.x * phase.x, upper.x);
    vec3 weightsY = vec3(lower.y, 0.75 - phase.y * phase.y, upper.y);
    vec2 center = floor(proj.xy * mapSize) + 0.5;
    float shadow = 0.0;
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y)
            shadow += weightsX[x] * weightsY[y] * sample_shadow(map, (center + vec2(x - 1, y - 1)) / mapSize, current_depth);
    }
    return shadow;
}

float shadowLevelWeight(mat4 lightSpace, vec4 cameraRange) {
    float weight = 0.0;
    if (cameraRange.w > 0.0) {
        vec4 local = lightSpace * vec4(v_worldPos, 1.0);
        vec3 clip = local.xyz / local.w;
        float edge = max(abs(clip.x), abs(clip.y));
        float distance = length(v_worldPos - cameraRange.xyz);
        if (abs(clip.z) < 1.0)
            weight = (1.0 - smoothstep(0.80, 0.96, edge)) * (1.0 - smoothstep(cameraRange.w * 0.75, cameraRange.w, distance));
    }
    return weight;
}

float terrainShadow(vec3 normal, float slope) {
    float weight = shadowLevelWeight(nearLightSpace, nearCameraRange);
    if (weight >= 1.0)
        return fetch_shadow(u_nearShadowMap, nearLightSpace, normal, slope);
    float mediumWeight = shadowLevelWeight(mediumLightSpace, mediumCameraRange);
    float wider;
    if (mediumWeight <= 0.0)
        wider = fetch_shadow(u_shadowMap, passLightSpace, normal, slope);
    else if (mediumWeight >= 1.0)
        wider = fetch_shadow(u_mediumShadowMap, mediumLightSpace, normal, slope);
    else
        wider = mix(fetch_shadow(u_shadowMap, passLightSpace, normal, slope), fetch_shadow(u_mediumShadowMap, mediumLightSpace, normal, slope), mediumWeight);
    if (weight <= 0.0)
        return wider;
    return mix(wider, fetch_shadow(u_nearShadowMap, nearLightSpace, normal, slope), weight);
}

vec3 cameraWorldPos() {
    vec3 translation = passView[3].xyz;
    return -vec3(dot(passView[0].xyz, translation), dot(passView[1].xyz, translation), dot(passView[2].xyz, translation));
}

vec2 wrapGrad(vec2 d) {
    float m2 = dot(d, d);
    return m2 > 16.0 ? d * sqrt(16.0 / m2) : d;
}

vec3 sampleAlbedo(sampler2DArray pack, vec2 uv, float layer, vec2 dx, vec2 dy, float tileBlend) {
    // Two continuous mappings: no per-tile random jumps or derivative seams.
    // The second scale/rotation breaks alignment with the primary 40 m repeat.
    const mat2 alternate = mat2(0.7986355, 0.6018150, -0.6018150, 0.7986355) * 1.37;
    vec3 first = textureGrad(pack, vec3(fract(uv), layer), dx, dy).rgb;
    vec3 second = textureGrad(pack, vec3(fract(alternate * uv + vec2(0.37, 0.61)), layer), alternate * dx, alternate * dy).rgb;
    return mix(first, second, tileBlend);
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

float surfaceMottle(vec3 pos, float detailAmt, out float tileBlend) {
    float coarse = valueNoise(pos * (1.0 / 360.0));
    float mid = valueNoise(pos * (1.0 / 120.0) + 19.0);
    // Fine variation is retained only when screen resolution can support it.
    tileBlend = smoothstep(0.22, 0.78, mid);
    float fine = 0.5;
    if (detailAmt > 0.0)
        fine = mix(0.5, valueNoise(pos * (1.0 / 28.0) * 5.3 + 41.0), detailAmt);
    return coarse * 0.54 + mid * 0.31 + fine * 0.15;
}

void main() {
    vec3 camera = cameraWorldPos();
    vec3 dir = normalize(v_objectPos);
    float altitude = v_drivers.x;
    float crater = v_drivers.y;
    float slope = 1.0 - clamp(dot(normalize(v_objectNormal), dir), 0.0, 1.0);
    float polar = abs(dir.y);
    float bowl = smoothstep(0.05, 0.85, clamp(-crater, 0.0, 1.4) / 1.4);
    float lowland = smoothstep(0.25, 1.50, max(-altitude, 0.0));
    float basinExposure = bowl * lowland;
    float flats = 1.0 - slope;
    float midLat = 1.0 - smoothstep(0.45, 0.82, polar);
    float highland = smoothstep(0.05, 0.55, altitude);

    float wIce =
        0.78 * smoothstep(0.52, 0.88, polar) +
        0.34 * smoothstep(0.18, 0.68, slope) * (0.35 + 0.65 * highland);
    float wOlivine = 0.55 * flats * midLat * (0.55 + 0.45 * (1.0 - highland));
    float wPyroxene = 0.48 * (0.35 + 0.65 * highland) * (0.55 + 0.45 * midLat);
    float wIron = (0.20 * bowl + 1.80 * basinExposure) * (0.45 + 0.55 * midLat);
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
    float pixelMeters = 0.5 * (length(dFdx(v_worldPos)) + length(dFdy(v_worldPos)));

    // Mean colors of the current crust BMPs in the existing GL_RGBA8 pipeline.
    // Fade resolved rock detail into its mean instead of showing repeated 40 m
    // tiles across the entire crater. Shadow filtering is independent of this fade.
    float detailAmt = 1.0 - smoothstep(0.35, 2.5, pixelMeters);
    float tileBlend;
    float mottle = surfaceMottle(v_objectPos, detailAmt, tileBlend);
    vec3 aIce = vec3(0.88, 0.90, 0.92);
    vec3 aOlivine = vec3(0.24, 0.25, 0.22);
    vec3 aPyroxene = vec3(0.28, 0.20, 0.16);
    vec3 aIron = vec3(0.54, 0.24, 0.09);
    if (detailAmt > 0.0) {
        aIce = mix(aIce, sampleAlbedo(u_albedoMap, uvCloseRaw, 0.0, closeDx, closeDy, tileBlend), detailAmt);
        aOlivine = mix(aOlivine, sampleAlbedo(u_albedoMap, uvCloseRaw, 1.0, closeDx, closeDy, tileBlend), detailAmt);
        aPyroxene = mix(aPyroxene, sampleAlbedo(u_albedoMap, uvCloseRaw, 2.0, closeDx, closeDy, tileBlend), detailAmt);
        aIron = mix(aIron, sampleAlbedo(u_albedoMap, uvCloseRaw, 6.0, closeDx, closeDy, tileBlend), detailAmt);
    }
    vec3 albedo = wIce * aIce + wOlivine * aOlivine + wPyroxene * aPyroxene + wIron * aIron;
    albedo *= mix(0.82, 1.22, smoothstep(0.16, 0.84, mottle)) * 1.14;
    // A restrained lift on inclined crater walls keeps their geometry readable
    // while direct light and the shadow maps still determine light direction.
    albedo *= mix(1.0, 1.12, smoothstep(0.12, 0.62, slope));

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
    // Convert the already sampled broad material pattern into resolved lighting
    // detail. Screen-space derivatives avoid extra noise or texture reads and
    // naturally disappear when a feature projects below a pixel.
    vec3 positionDx = dFdx(v_worldPos);
    vec3 positionDy = dFdy(v_worldPos);
    vec3 gradientAcross = cross(positionDy, N);
    vec3 gradientDown = cross(N, positionDx);
    float determinant = dot(positionDx, gradientAcross);
    vec3 surfaceGradient = sign(determinant) *
        (dFdx(mottle) * gradientAcross + dFdy(mottle) * gradientDown);
    float bumpMeters = mix(46.0, 8.0, detailAmt);
    N = normalize(abs(determinant) * N - bumpMeters * surfaceGradient);
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
    float shadow = terrainShadow(N, lightSlope);
    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 ambient = (kD * albedo + F0 * 0.22) * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain)) * cavity;
    vec3 direct = (kD * albedo * lunarLambert + specular) * passPrimaryLightColorRange.rgb * shadow * (lightGain / (1.0 + lightGain)) * cavity;
    FragColor = vec4(ambient + direct, 1.0);
    BloomMask = 0.0;
}
