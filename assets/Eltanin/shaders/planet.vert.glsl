#version 460 core

layout (location = 0) in vec3 aPos;

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

struct GpuTile {
    ivec4 loc;
    ivec4 neighbors;
};

layout(std430, binding = 8) readonly buffer PatchBuffer {
    GpuTile tiles[];
};

layout(binding = 2) uniform sampler2DArray u_heightMap;
layout(binding = 5) uniform sampler2DArray u_coverMap;

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec3 v_objectNormal;
out vec3 v_objectPos;
flat out uvec3 v_layerPack;
flat out vec3 v_seed;
out vec3 v_bary;

vec3 onDiamond(float u, float v, vec3 top, vec3 right, vec3 bottom, vec3 left) {
    if (u + v <= 1.0)
        return (1.0 - u - v) * top + u * right + v * left;
    return (1.0 - v) * right + (u + v - 1.0) * bottom + (1.0 - u) * left;
}

vec3 pointAt(int diamond, ivec2 local) {
    local = clamp(local, ivec2(0), ivec2(fieldSpan - 1));
    ivec4 corners = diamonds[diamond];
    float segments = float(max(fieldSpan - 1, 1));
    vec3 dir = normalize(onDiamond(float(local.x) / segments, float(local.y) / segments, shell[corners.x].xyz, shell[corners.y].xyz, shell[corners.z].xyz, shell[corners.w].xyz));
    return dir * (fieldRadius + texelFetch(u_heightMap, ivec3(local, diamond), 0).r * fieldAmplitude);
}

uint paletteAt(int diamond, ivec2 local) {
    local = clamp(local, ivec2(0), ivec2(fieldSpan - 1));
    vec4 cover = texelFetch(u_coverMap, ivec3(local, diamond), 0);
    return uint(cover.r * 255.0 + 0.5) | (uint(cover.g * 255.0 + 0.5) << 8) | (uint(cover.b * 255.0 + 0.5) << 16) | (uint(cover.a * 255.0 + 0.5) << 24);
}

void main() {
    GpuTile tile = tiles[gl_InstanceID];
    int selfStep = max(tile.loc.w, 1);
    int corner = int(aPos.z + 0.5);
    ivec2 idx = ivec2(aPos.xy + 0.5);
    ivec2 local = tile.loc.yz + idx * selfStep;
    ivec2 slotA = local;
    ivec2 slotB = local;
    ivec2 slotC = local;
    if (corner == 0) {
        slotB = local + ivec2(selfStep, 0);
        slotC = local + ivec2(0, selfStep);
        v_bary = vec3(1.0, 0.0, 0.0);
    } else if (corner == 1) {
        slotA = local + ivec2(-selfStep, 0);
        slotC = local + ivec2(-selfStep, selfStep);
        v_bary = vec3(0.0, 1.0, 0.0);
    } else if (corner == 2) {
        slotA = local + ivec2(0, -selfStep);
        slotB = local + ivec2(selfStep, -selfStep);
        v_bary = vec3(0.0, 0.0, 1.0);
    } else if (corner == 3) {
        slotB = local + ivec2(0, selfStep);
        slotC = local + ivec2(-selfStep, selfStep);
        v_bary = vec3(1.0, 0.0, 0.0);
    } else if (corner == 4) {
        slotA = local + ivec2(0, -selfStep);
        slotC = local + ivec2(-selfStep, 0);
        v_bary = vec3(0.0, 1.0, 0.0);
    } else {
        slotA = local + ivec2(selfStep, -selfStep);
        slotB = local + ivec2(selfStep, 0);
        v_bary = vec3(0.0, 0.0, 1.0);
    }
    int normalStep = max(selfStep, 1);
    vec3 objectPos = pointAt(tile.loc.x, local);
    vec3 tangentU = pointAt(tile.loc.x, local + ivec2(normalStep, 0)) - pointAt(tile.loc.x, local - ivec2(normalStep, 0));
    vec3 tangentV = pointAt(tile.loc.x, local + ivec2(0, normalStep)) - pointAt(tile.loc.x, local - ivec2(0, normalStep));
    vec3 normal = cross(tangentU, tangentV);
    float mag = length(normal);
    vec3 radial = normalize(objectPos);
    if (mag < 1.0e-8)
        normal = radial;
    else {
        normal /= mag;
        if (dot(normal, radial) < 0.0)
            normal = -normal;
    }
    vec4 worldPos = actorModel * vec4(objectPos, 1.0);
    v_worldPos = worldPos.xyz;
    v_objectPos = objectPos;
    v_objectNormal = normal;
    v_worldNormal = normalize(mat3(transpose(inverse(actorModel))) * normal);
    v_layerPack = uvec3(paletteAt(tile.loc.x, slotA), paletteAt(tile.loc.x, slotB), paletteAt(tile.loc.x, slotC));
    v_seed = objectPos;
    gl_Position = passProjection * passView * worldPos;
}
