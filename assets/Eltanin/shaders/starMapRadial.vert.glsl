#version 460 core
layout (location = 0) in vec3 aPos;

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

out vec3 vLocalPos;

void main() {
    ivec4 pose = poses[gl_BaseInstance];
    vec3 localPosition = aPos + vec3(pose.xyz) * actorLatticePattern.x;
    vLocalPos = localPosition;
    gl_Position = passProjection * passView * actorModel * vec4(localPosition, 1.0);
}
