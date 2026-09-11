#version 460 core

in vec3 worldPos;
in vec3 worldNormal;
noperspective in vec3 barycentric;
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

float shadowAt(sampler2D map, mat4 lightSpace, vec3 normal, float slope) {
    // Match production world-texel bias and continuous comparison filtering.
    vec2 mapSize = vec2(textureSize(map, 0));
    vec3 lightX = vec3(lightSpace[0].x, lightSpace[1].x, lightSpace[2].x);
    vec3 lightY = vec3(lightSpace[0].y, lightSpace[1].y, lightSpace[2].y);
    vec3 lightZ = vec3(lightSpace[0].z, lightSpace[1].z, lightSpace[2].z);
    float texelMeters = max(2.0 / (mapSize.x * length(lightX)), 2.0 / (mapSize.y * length(lightY)));
    vec4 lightPos = lightSpace * vec4(worldPos + normal * texelMeters * (0.5 + slope), 1.0);
    vec3 projected = lightPos.xyz / lightPos.w * 0.5 + 0.5;
    if (projected.z < 0.0 || projected.z > 1.0 || any(lessThan(projected.xy, vec2(0.0))) || any(greaterThan(projected.xy, vec2(1.0))))
        return 1.0;
    float depth = projected.z - max(0.0000002, texelMeters * length(lightZ) * 0.05);
    vec2 phase = fract(projected.xy * mapSize) - 0.5;
    vec2 lower = 0.5 * (0.5 - phase) * (0.5 - phase);
    vec2 upper = 0.5 * (0.5 + phase) * (0.5 + phase);
    vec3 weightsX = vec3(lower.x, 0.75 - phase.x * phase.x, upper.x);
    vec3 weightsY = vec3(lower.y, 0.75 - phase.y * phase.y, upper.y);
    vec2 center = floor(projected.xy * mapSize) + 0.5;
    float shadow = 0.0;
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            float closest = texture(map, (center + vec2(x - 1, y - 1)) / mapSize).r;
            shadow += weightsX[x] * weightsY[y] * (depth > closest ? 0.0 : 1.0);
        }
    }
    return shadow;
}

float shadowLevelWeight(mat4 lightSpace, vec4 cameraRange) {
    float weight = 0.0;
    if (cameraRange.w > 0.0) {
        vec4 local = lightSpace * vec4(worldPos, 1.0);
        vec3 clip = local.xyz / local.w;
        float edge = max(abs(clip.x), abs(clip.y));
        float distance = length(worldPos - cameraRange.xyz);
        if (abs(clip.z) < 1.0)
            weight = (1.0 - smoothstep(0.80, 0.96, edge)) * (1.0 - smoothstep(cameraRange.w * 0.75, cameraRange.w, distance));
    }
    return weight;
}

float terrainShadow(vec3 normal, float slope) {
    float weight = shadowLevelWeight(nearLightSpace, nearCameraRange);
    if (weight >= 1.0)
        return shadowAt(u_nearShadowMap, nearLightSpace, normal, slope);
    float mediumWeight = shadowLevelWeight(mediumLightSpace, mediumCameraRange);
    float wider;
    if (mediumWeight <= 0.0)
        wider = shadowAt(u_shadowMap, passLightSpace, normal, slope);
    else if (mediumWeight >= 1.0)
        wider = shadowAt(u_mediumShadowMap, mediumLightSpace, normal, slope);
    else
        wider = mix(shadowAt(u_shadowMap, passLightSpace, normal, slope), shadowAt(u_mediumShadowMap, mediumLightSpace, normal, slope), mediumWeight);
    if (weight <= 0.0)
        return wider;
    return mix(wider, shadowAt(u_nearShadowMap, nearLightSpace, normal, slope), weight);
}

void main() {
    int mode = int(actorHeat.x + 0.5);
    vec3 normal = normalize(worldNormal);
    if (mode == 3) {
        vec3 face = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
        normal = dot(face, normal) < 0.0 ? -face : face;
    }
    BloomMask = 0.0;
    if (mode == 4) {
        FragColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }
    vec3 lightDir = normalize(passPrimaryLightPositionIntensity.xyz - worldPos * float(passPrimaryLightColorRange.w > 0.0));
    float light = max(dot(normal, lightDir), 0.0);
    if (mode == 6) {
        // Use exactly the receiver's selection weight, including distance and Z.
        float weight = shadowLevelWeight(nearLightSpace, nearCameraRange);
        float mediumWeight = shadowLevelWeight(mediumLightSpace, mediumCameraRange);
        vec3 coverage = vec3(1.0, 0.42, 0.03);
        if (nearCameraRange.w <= 0.0 && mediumCameraRange.w <= 0.0)
            coverage = vec3(0.4);
        else if (weight >= 1.0)
            coverage = vec3(0.04, 0.85, 0.12);
        else if (weight <= 0.0 && mediumWeight >= 1.0)
            coverage = vec3(0.03, 0.75, 0.9);
        else if (weight <= 0.0 && mediumWeight <= 0.0)
            coverage = vec3(0.04, 0.22, 1.0);
        FragColor = vec4(coverage * (0.45 + 0.55 * light), 1.0);
        return;
    }
    float shadow = mode == 2 ? terrainShadow(normal, 1.0 - light * smoothstep(0.0, sin(radians(15.0)), light)) : 1.0;
    vec3 color = vec3(0.55) * (0.18 + 0.82 * light * shadow);
    if (mode == 5) {
        vec3 width = max(fwidth(barycentric), vec3(0.000001));
        vec3 interior = smoothstep(vec3(0.0), width * 1.2, barycentric);
        float edge = 1.0 - min(interior.x, min(interior.y, interior.z));
        color = mix(color, vec3(0.02, 0.12, 0.15), edge);
    }
    FragColor = vec4(color, 1.0);
}
