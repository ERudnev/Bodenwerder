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

layout(std430, binding = 8) readonly buffer PatchBuffer {
    ivec4 tiles[];
};

layout(binding = 2) uniform sampler2D u_heightMap;

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec3 v_objectPos;
out vec2 v_atlas;

vec3 onDiamond(float u, float v, vec3 top, vec3 right, vec3 bottom, vec3 left) {
    if (u + v <= 1.0)
        return (1.0 - u - v) * top + u * right + v * left;
    return (1.0 - v) * right + (u + v - 1.0) * bottom + (1.0 - u) * left;
}

ivec2 atlasOf(int diamond, ivec2 local) {
    local = clamp(local, ivec2(0), ivec2(fieldSpan - 1));
    return ivec2((diamond % 5) * fieldSpan + local.x, (diamond / 5) * fieldSpan + local.y);
}

vec3 pointAt(int diamond, ivec2 local) {
    local = clamp(local, ivec2(0), ivec2(fieldSpan - 1));
    ivec4 corners = diamonds[diamond];
    ivec2 atlas = atlasOf(diamond, local);
    float segments = float(max(fieldSpan - 1, 1));
    vec3 dir = normalize(onDiamond(float(local.x) / segments, float(local.y) / segments, shell[corners.x].xyz, shell[corners.y].xyz, shell[corners.z].xyz, shell[corners.w].xyz));
    return dir * (fieldRadius + texelFetch(u_heightMap, atlas, 0).r * fieldAmplitude);
}

void main() {
    ivec4 tile = tiles[gl_InstanceID];
    ivec2 local = clamp(ivec2(tile.yz) + ivec2(aPos.xy) * tile.w, ivec2(0), ivec2(fieldSpan - 1));
    vec3 objectPos = pointAt(tile.x, local);
    vec3 tangentU = pointAt(tile.x, local + ivec2(1, 0)) - pointAt(tile.x, local - ivec2(1, 0));
    vec3 tangentV = pointAt(tile.x, local + ivec2(0, 1)) - pointAt(tile.x, local - ivec2(0, 1));
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
    v_worldNormal = normalize(mat3(transpose(inverse(actorModel))) * normal);
    v_atlas = vec2(atlasOf(tile.x, local));
    gl_Position = passProjection * passView * worldPos;
}
