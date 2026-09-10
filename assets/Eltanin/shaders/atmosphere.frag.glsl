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
const int sunSamples = 2;
const float visualExtinction = 5.0e-8;
const vec3 scatterBeta = vec3(0.45, 1.00, 2.55);

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

float densityAt(vec3 pos, vec3 planetCenter, float planetRadius, float scaleHeight, float seaDensity) {
    float altitude = length(pos - planetCenter) - planetRadius;
    return seaDensity * exp(-max(altitude, 0.0) / scaleHeight);
}

float opticalAlong(vec3 origin, vec3 dir, float tEnter, float tExit, vec3 planetCenter, float planetRadius, float scaleHeight, float seaDensity) {
    tEnter = max(tEnter, 0.0);
    if (tExit <= tEnter)
        return 0.0;
    float stepLength = (tExit - tEnter) / float(sunSamples);
    float optical = 0.0;
    for (int i = 0; i < sunSamples; ++i) {
        float t = tEnter + stepLength * (float(i) + 0.5);
        optical += densityAt(origin + dir * t, planetCenter, planetRadius, scaleHeight, seaDensity);
    }
    return optical * stepLength * visualExtinction;
}

float sunLight(vec3 pos, vec3 sunDir, vec3 planetCenter) {
    vec3 radial = pos - planetCenter;
    float mu = dot(radial / max(length(radial), 1.0e-6), sunDir);
    return smoothstep(-0.42, 0.18, mu);
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
    float tAtmoExit;
    if (!intersectSphere(camPos, rayDir, planetCenter, atmosphereRadius, tEnter, tAtmoExit))
        discard;

    float hitDist = sceneDistance(camPos, invViewProj);
    tEnter = max(tEnter, 0.0);
    float tExit = min(tAtmoExit, hitDist);
    bool hitGround = hitDist < tAtmoExit;
    if (tExit <= tEnter)
        discard;

    float shell = max(atmosphereRadius - planetRadius, 1.0);
    float scaleHeight = shell * 0.25;
    vec3 sunPos = passPrimaryLightPositionIntensity.xyz;
    bool pointSun = passPrimaryLightColorRange.w > 0.0;
    vec3 sunColor = passPrimaryLightColorRange.rgb * max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 toCam = camPos - planetCenter;
    float tLit = clamp(-dot(toCam, rayDir), tEnter, tExit);
    vec3 posLit = camPos + rayDir * tLit;
    vec3 sunDir = pointSun ? sunPos - posLit : sunPos;
    float sunLen = length(sunDir);
    sunDir = sunLen > 1.0e-6 ? sunDir / sunLen : vec3(0.0, 1.0, 0.0);
    float shadow = sunLight(posLit, sunDir, planetCenter);
    float sunEnter;
    float sunLeave;
    float sunTau = 0.0;
    if (intersectSphere(posLit, sunDir, planetCenter, atmosphereRadius, sunEnter, sunLeave))
        sunTau = opticalAlong(posLit, sunDir, sunEnter, sunLeave, planetCenter, planetRadius, scaleHeight, seaDensity);
    vec3 transSun = exp(-scatterBeta * sunTau) * shadow;
    vec3 scatter = vec3(0.0);
    float viewOptical = 0.0;
    float stepLength = (tExit - tEnter) / float(densitySamples);
    for (int i = 0; i < densitySamples; ++i) {
        float t = tEnter + stepLength * (float(i) + 0.5);
        vec3 pos = camPos + rayDir * t;
        float rho = densityAt(pos, planetCenter, planetRadius, scaleHeight, seaDensity);
        vec3 transView = exp(-scatterBeta * viewOptical);
        float mu = dot(rayDir, sunDir);
        float phase = 0.75 + 0.75 * mu * mu;
        scatter += rho * visualExtinction * stepLength * transSun * transView * sunColor * phase * actorAlbedoOpacity.rgb;
        viewOptical += rho * visualExtinction * stepLength;
    }

    float towardSun = max(dot(rayDir, sunDir), 0.0);
    float chord = tExit - tEnter;
    float longPath = 1.0 - exp(-chord / (scaleHeight * 33.0));
    vec3 toPlanet = planetCenter - camPos;
    float alongView = dot(toPlanet, rayDir);
    float impact = sqrt(max(dot(toPlanet, toPlanet) - alongView * alongView, 0.0));
    float rimFade = 1.0 - smoothstep(atmosphereRadius - scaleHeight * 3.7, atmosphereRadius, impact);
    float aureole = pow(towardSun, mix(1850.0, 370.0, longPath)) * longPath * rimFade;
    scatter += transSun * sunColor * actorAlbedoOpacity.rgb * aureole * 0.55;
    scatter *= rimFade;

    float absorb = (1.0 - exp(-viewOptical)) * rimFade;
    if (hitGround)
        absorb *= mix(0.2, 1.0, sunLight(camPos + rayDir * tExit, sunDir, planetCenter));
    if (absorb < 0.001 && dot(scatter, vec3(1.0)) < 0.001)
        discard;

    FragColor = vec4(scatter, absorb);
    BloomMask = max(scatter.r, max(scatter.g, scatter.b)) * 0.22 + aureole * 0.85;
}
