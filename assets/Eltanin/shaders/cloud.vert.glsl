#version 460 core

layout (location = 0) in vec3 aPos;

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

layout(std430, binding = 8) readonly buffer PoseBuffer {
    ivec4 poses[];
};

out vec3 v_worldPos;

void main() {
    mat4 invView = inverse(passView);
    vec3 camPos = invView[3].xyz / max(invView[3].w, 1.0e-6);
    vec3 worldPos = camPos + normalize(aPos) * 100.0;
    v_worldPos = worldPos;
    gl_Position = passProjection * passView * vec4(worldPos, 1.0);
}
