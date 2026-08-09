#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv0;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
};

layout(std430, binding = 8) readonly buffer PoseBuffer {
    ivec4 poses[];
};

layout(std430, binding = 0) readonly buffer AtlasEntries {
    ivec4 data[];
};

uniform vec2 u_inverseAtlasSize;

out vec2 v_uv0;

void main() {
    int spriteIndex = int(actorSpriteIndex);
    ivec4 rect = data[spriteIndex * 2];
    ivec4 extra = data[spriteIndex * 2 + 1];

    vec2 spriteMin = vec2(rect.xy);
    vec2 spriteSize = vec2(rect.zw);
    vec2 pivotDown = vec2(extra.xy) - spriteMin;
    vec2 pivot = vec2(pivotDown.x, spriteSize.y - pivotDown.y);
    vec2 localPixels = aUv0 * spriteSize - pivot;

    ivec4 pose = poses[gl_BaseInstance];
    vec3 localPosition = vec3(localPixels, aPos.z) + vec3(pose.xyz) * actorLatticePattern.x;
    vec4 worldPos = actorModel * vec4(localPosition, 1.0);
    gl_Position = passProjection * passView * worldPos;

    vec2 uvMin = spriteMin * u_inverseAtlasSize;
    vec2 uvMax = (spriteMin + spriteSize) * u_inverseAtlasSize;
    v_uv0 = vec2(
        mix(uvMin.x, uvMax.x, aUv0.x),
        mix(uvMin.y, uvMax.y, aUv0.y));
}
