#version 460 core

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
    vec4 passShutter;
};

layout(binding = 1) uniform sampler2D u_sceneDepth;

const int densitySamples = 4;
const float visualExtinction = 5.0e-8;
const float earthSeaDensity = 1200.0;

bool intersectSphere(vec3 origin, vec3 dir, vec3 center, float radius, out float tEnter, out float tExit) {
    vec3 offset = origin - center;
    float b = dot(offset, dir);
    float c = dot(offset, offset) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0)
        return false;
    float s = sqrt(max(disc, 0.0));
    tEnter = -b - s;
    tExit = -b + s;
    return tExit > 0.0;
}

vec3 cameraWorld(mat4 invView) {
    return invView[3].xyz / max(invView[3].w, 1.0e-6);
}

vec3 pixelRayDir(vec3 camPos, mat4 invViewProj) {
    ivec2 size = textureSize(u_sceneDepth, 0);
    vec2 ndc = gl_FragCoord.xy / vec2(size) * 2.0 - 1.0;
    vec4 worldFar = invViewProj * vec4(ndc, 1.0, 1.0);
    worldFar /= max(worldFar.w, 1.0e-6);
    return normalize(worldFar.xyz - camPos);
}

float sceneDistance(vec3 camPos, mat4 invViewProj) {
    ivec2 size = textureSize(u_sceneDepth, 0);
    vec2 uv = gl_FragCoord.xy / vec2(size);
    float depth = texture(u_sceneDepth, uv).r;
    vec4 world = invViewProj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    world /= max(world.w, 1.0e-6);
    return length(world.xyz - camPos);
}

void main() {
    if (gl_FrontFacing)
        discard;

    float planetRadius = actorHeat.x;
    float atmosphereRadius = actorHeat.y;
    float seaDensity = actorAlbedoOpacity.a;
    if (atmosphereRadius <= planetRadius || seaDensity <= 0.0)
        discard;

    mat4 invView = inverse(passView);
    mat4 invViewProj = inverse(passProjection * passView);
    vec3 camPos = cameraWorld(invView);
    vec3 rayDir = pixelRayDir(camPos, invViewProj);
    vec3 planetCenter = actorModel[3].xyz / max(actorModel[3].w, 1.0e-6);

    float tEnter;
    float tExit;
    if (!intersectSphere(camPos, rayDir, planetCenter, atmosphereRadius, tEnter, tExit))
        discard;

    tEnter = max(tEnter, 0.0);
    tExit = min(tExit, sceneDistance(camPos, invViewProj));
    if (tExit <= tEnter)
        discard;

    float shell = max(atmosphereRadius - planetRadius, 1.0);
    float scaleHeight = shell * 0.25;
    float optical = 0.0;
    float stepLength = (tExit - tEnter) / float(densitySamples);
    for (int i = 0; i < densitySamples; ++i) {
        float t = tEnter + stepLength * (float(i) + 0.5);
        float altitude = length(camPos + rayDir * t - planetCenter) - planetRadius;
        optical += seaDensity * exp(-max(altitude, 0.0) / scaleHeight);
    }
    optical *= stepLength * visualExtinction;
    optical = min(optical, 1.6);

    float absorb = 1.0 - exp(-optical);
    if (absorb < 0.001)
        discard;

    vec3 sunDir = passPrimaryLightPositionIntensity.xyz;
    float sunLen = length(sunDir);
    sunDir = sunLen > 1.0e-6 ? sunDir / sunLen : vec3(0.0, 1.0, 0.0);
    float sunAlong = max(dot(rayDir, sunDir), 0.0);
    float limb = pow(sunAlong, 4.0);
    vec3 scatterColor = actorAlbedoOpacity.rgb;
    vec3 sunColor = passPrimaryLightColorRange.rgb * max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 glow = scatterColor * (sunColor * (0.18 + 1.35 * limb) + passAmbientColorIntensity.rgb * 0.35 + vec3(0.12, 0.16, 0.22));
    glow *= (seaDensity / max(earthSeaDensity, 1.0)) * (optical / max(absorb, 1.0e-4));

    FragColor = vec4(glow * absorb, absorb);
    BloomMask = absorb * (0.12 + 0.55 * limb);
}
