#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aBarycentric;

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

out vec3 worldPos;
out vec3 worldNormal;
noperspective out vec3 barycentric;

void main() {
    // Planetoid patches contain planet-local positions, with one identity instance.
    worldPos = (actorModel * vec4(aPos, 1.0)).xyz;
    worldNormal = normalize(mat3(transpose(inverse(actorModel))) * aNormal);
    barycentric = vec3(aBarycentric, 1.0 - aBarycentric.x - aBarycentric.y);
    gl_Position = passProjection * passView * vec4(worldPos, 1.0);
}
