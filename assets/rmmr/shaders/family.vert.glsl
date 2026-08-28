#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

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

layout(std430, binding = 8) readonly buffer InstanceModelBuffer {
    mat4 instanceModel[];
};

out vec3 v_worldPos;
out vec3 v_worldNormal;

void main() {
    mat4 model = instanceModel[gl_InstanceID];
    vec4 worldPos = model * vec4(aPos, 1.0);
    v_worldPos = worldPos.xyz;

    mat3 normalMat = mat3(transpose(inverse(model)));
    v_worldNormal = normalize(normalMat * aNormal);

    gl_Position = passProjection * passView * worldPos;
}
