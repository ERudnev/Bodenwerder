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
layout(binding = 2) uniform sampler2DArray u_heightMap;

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
    vec4 worldFar = invViewProj * vec4(ndc, 0.0, 1.0);
    if (abs(worldFar.w) < 1.0e-6)
        return normalize(worldFar.xyz);
    worldFar /= worldFar.w;
    return normalize(worldFar.xyz - camPos);
}

float sceneDistance(vec3 camPos, mat4 invViewProj) {
    ivec2 size = textureSize(u_sceneDepth, 0);
    vec2 uv = gl_FragCoord.xy / vec2(size);
    float depth = texture(u_sceneDepth, uv).r;
    if (depth <= 1.0e-7)
        return 1.0e12;
    vec4 world = invViewProj * vec4(uv * 2.0 - 1.0, depth, 1.0);
    world /= max(world.w, 1.0e-6);
    return length(world.xyz - camPos);
}

vec3 icosaVertex(int index) {
    if (index == 0)
        return vec3(0.0, 1.0, 0.0);
    if (index == 11)
        return vec3(0.0, -1.0, 0.0);
    float lat = atan(0.5);
    int spoke = index < 6 ? index - 1 : index - 6;
    float turn = float(spoke) * 1.2566370614;
    if (index >= 6)
        turn += 0.6283185307;
    float ring = cos(lat);
    float y = index < 6 ? sin(lat) : -sin(lat);
    return vec3(cos(turn) * ring, y, sin(turn) * ring);
}

void icosaDiamond(int diamond, out int top, out int right, out int bottom, out int left) {
    int spoke = diamond < 5 ? diamond : diamond - 5;
    int next = (spoke + 1) % 5;
    if (diamond < 5) {
        top = 0;
        right = 1 + next;
        bottom = 6 + spoke;
        left = 1 + spoke;
    } else {
        top = 1 + next;
        right = 6 + next;
        bottom = 11;
        left = 6 + spoke;
    }
    vec3 t = icosaVertex(top);
    vec3 r = icosaVertex(right);
    vec3 l = icosaVertex(left);
    if (dot(cross(r - t, l - t), t) < 0.0) {
        int swap = right;
        right = left;
        left = swap;
    }
}

vec4 sampleWeather(vec3 direction) {
    vec3 dir = normalize(direction);
    int bestFace = 0;
    float bestDot = -2.0;
    int bestTop = 0;
    int bestRight = 0;
    int bestBottom = 0;
    int bestLeft = 0;
    bool bestLower = false;
    vec3 bestA = dir;
    vec3 bestB = dir;
    vec3 bestC = dir;
    vec3 bestN = dir;
    for (int face = 0; face < 20; ++face) {
        int diamond = face % 10;
        bool lower = face >= 10;
        int top, right, bottom, left;
        icosaDiamond(diamond, top, right, bottom, left);
        vec3 a = lower ? icosaVertex(right) : icosaVertex(top);
        vec3 b = lower ? icosaVertex(bottom) : icosaVertex(right);
        vec3 c = icosaVertex(left);
        vec3 n = normalize(cross(b - a, c - a));
        if (dot(n, a + b + c) < 0.0)
            n = -n;
        float facing = dot(dir, n);
        if (facing > bestDot) {
            bestDot = facing;
            bestFace = face;
            bestLower = lower;
            bestTop = top;
            bestRight = right;
            bestBottom = bottom;
            bestLeft = left;
            bestA = a;
            bestB = b;
            bestC = c;
            bestN = n;
        }
    }
    vec3 point = dir * (dot(bestA, bestN) / max(dot(dir, bestN), 1.0e-6));
    vec3 edgeAB = bestB - bestA;
    vec3 edgeAC = bestC - bestA;
    vec3 fromA = point - bestA;
    float d00 = dot(edgeAB, edgeAB);
    float d01 = dot(edgeAB, edgeAC);
    float d11 = dot(edgeAC, edgeAC);
    float d20 = dot(fromA, edgeAB);
    float d21 = dot(fromA, edgeAC);
    float denom = max(d00 * d11 - d01 * d01, 1.0e-8);
    float baryB = (d11 * d20 - d01 * d21) / denom;
    float baryC = (d00 * d21 - d01 * d20) / denom;
    float baryA = 1.0 - baryB - baryC;
    float u = bestLower ? 1.0 - baryC : baryB;
    float v = bestLower ? 1.0 - baryA : baryC;
    int diamond = bestFace % 10;
    return texture(u_heightMap, vec3(clamp(u, 0.0, 1.0), clamp(v, 0.0, 1.0), float(diamond)));
}

float hash31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i);
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));
    float n00 = mix(n000, n100, u.x);
    float n10 = mix(n010, n110, u.x);
    float n01 = mix(n001, n101, u.x);
    float n11 = mix(n011, n111, u.x);
    return mix(mix(n00, n10, u.y), mix(n01, n11, u.y), u.z);
}

float fbm2(vec3 p) {
    return valueNoise(p) * 0.65 + valueNoise(p * 2.09 + 17.2) * 0.35;
}

void main() {
    if (gl_FrontFacing)
        discard;

    float innerRadius = actorHeat.x;
    float outerRadius = actorHeat.y;
    float channel = actorLatticePattern.y;
    if (outerRadius <= innerRadius)
        discard;

    mat4 invView = inverse(passView);
    mat4 invViewProj = inverse(passProjection * passView);
    vec3 camPos = cameraWorld(invView);
    vec3 rayDir = pixelRayDir(camPos, invViewProj);
    vec3 planetCenter = actorModel[3].xyz / max(actorModel[3].w, 1.0e-6);
    float camR = length(camPos - planetCenter);

    float tOut0;
    float tOut1;
    if (!intersectSphere(camPos, rayDir, planetCenter, outerRadius, tOut0, tOut1) || tOut1 <= 0.0)
        discard;
    float tIn0;
    float tIn1;
    bool hitInner = intersectSphere(camPos, rayDir, planetCenter, innerRadius, tIn0, tIn1);

    float tEnter;
    float tExit;
    if (camR >= outerRadius) {
        tEnter = tOut0;
        tExit = hitInner ? tIn0 : tOut1;
    } else if (camR > innerRadius) {
        tEnter = 0.0;
        tExit = (hitInner && tIn0 > 0.0) ? tIn0 : tOut1;
    } else {
        if (!hitInner)
            discard;
        tEnter = tIn1;
        tExit = tOut1;
    }
    tEnter = max(tEnter, 0.0);
    float hitDist = sceneDistance(camPos, invViewProj);
    tExit = min(tExit, hitDist);
    if (tExit <= tEnter)
        discard;

    vec3 sunPos = passPrimaryLightPositionIntensity.xyz;
    bool pointSun = passPrimaryLightColorRange.w > 0.0;
    vec3 sunColor = passPrimaryLightColorRange.rgb * max(passPrimaryLightPositionIntensity.w, 0.0);
    vec3 mid = camPos + rayDir * mix(tEnter, tExit, 0.5);
    vec3 sunDir = pointSun ? sunPos - mid : sunPos;
    float sunLen = length(sunDir);
    sunDir = sunLen > 1.0e-6 ? sunDir / sunLen : vec3(0.0, 1.0, 0.0);
    mat3 toObject = inverse(mat3(actorModel));
    float thickness = max(outerRadius - innerRadius, 1.0);
    float slant = clamp((tExit - tEnter) / thickness, 0.0, 4.0);
    vec3 local = toObject * (mid - planetCenter);
    float height = clamp((length(local) - innerRadius) / thickness, 0.0, 1.0);
    vec3 dir = normalize(local);
    vec3 noiseP = dir * mix(14.0, 22.0, clamp(channel, 0.0, 1.0)) + vec3(0.0, height * 3.4, 0.0);
    noiseP += (vec3(valueNoise(noiseP), valueNoise(noiseP + 19.1), valueNoise(noiseP + 47.3)) - 0.5) * mix(0.22, 0.55, clamp(channel, 0.0, 1.0));
    float grain = fbm2(noiseP);
    float billow = 1.0 - abs(grain * 2.0 - 1.0);
    float carve = mix(mix(0.55, 1.0, grain), smoothstep(0.22, 0.68, billow), clamp(channel, 0.0, 1.0));
    float profile = smoothstep(0.0, 0.18, height) * smoothstep(1.0, 0.52, height);
    vec4 weather = sampleWeather(local);
    float tracer = mix(weather.a, weather.b, clamp(channel, 0.0, 1.0));
    float density = mix(tracer * tracer, tracer, clamp(channel, 0.0, 1.0)) * carve * profile;
    if (density < 0.02)
        discard;
    float optical = density * mix(0.22, 0.90, clamp(channel, 0.0, 1.0)) * slant;
    float lit = smoothstep(-0.15, 0.35, dot(normalize(mid - planetCenter), sunDir));
    vec3 scatter = optical * sunColor * actorAlbedoOpacity.rgb * (0.35 + 0.65 * lit);
    float absorb = (1.0 - exp(-optical)) * 0.88;
    if (absorb < 0.004 && dot(scatter, vec3(1.0)) < 0.004)
        discard;
    FragColor = vec4(scatter, absorb);
    BloomMask = max(scatter.r, max(scatter.g, scatter.b)) * 0.18;
}
