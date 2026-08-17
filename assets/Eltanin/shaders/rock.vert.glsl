#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in uvec2 aMix0;

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

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec3 v_objectPos;
out vec4 v_mix0;
out vec4 v_mix1;
out vec4 v_mix2;
out vec4 v_mix3;

const ivec3 orientationRow0[24] = ivec3[24](
    ivec3(1, 0, 0), ivec3(1, 0, 0), ivec3(1, 0, 0), ivec3(1, 0, 0),
    ivec3(0, 0, -1), ivec3(0, 0, -1), ivec3(0, 0, -1), ivec3(0, 0, -1),
    ivec3(-1, 0, 0), ivec3(-1, 0, 0), ivec3(-1, 0, 0), ivec3(-1, 0, 0),
    ivec3(0, 0, 1), ivec3(0, 0, 1), ivec3(0, 0, 1), ivec3(0, 0, 1),
    ivec3(0, 1, 0), ivec3(0, 1, 0), ivec3(0, 1, 0), ivec3(0, 1, 0),
    ivec3(0, -1, 0), ivec3(0, -1, 0), ivec3(0, -1, 0), ivec3(0, -1, 0)
);

const ivec3 orientationRow1[24] = ivec3[24](
    ivec3(0, 1, 0), ivec3(0, 0, 1), ivec3(0, -1, 0), ivec3(0, 0, -1),
    ivec3(0, 1, 0), ivec3(1, 0, 0), ivec3(0, -1, 0), ivec3(-1, 0, 0),
    ivec3(0, 1, 0), ivec3(0, 0, -1), ivec3(0, -1, 0), ivec3(0, 0, 1),
    ivec3(0, 1, 0), ivec3(-1, 0, 0), ivec3(0, -1, 0), ivec3(1, 0, 0),
    ivec3(0, 0, 1), ivec3(1, 0, 0), ivec3(0, 0, -1), ivec3(-1, 0, 0),
    ivec3(0, 0, -1), ivec3(1, 0, 0), ivec3(0, 0, 1), ivec3(-1, 0, 0)
);

mat3 orientationMatrix(int orientation) {
    vec3 r0 = vec3(orientationRow0[orientation]);
    vec3 r1 = vec3(orientationRow1[orientation]);
    vec3 r2 = cross(r0, r1);
    return transpose(mat3(r0, r1, r2));
}

void main() {
    ivec4 pose = poses[gl_BaseInstance];
    mat3 localRotation = orientationMatrix(pose.w);
    vec3 localPosition = localRotation * aPos + vec3(pose.xyz) * actorLatticePattern.x;
    vec4 worldPos = actorModel * vec4(localPosition, 1.0);
    v_worldPos = worldPos.xyz;
    v_objectPos = localPosition;
    v_mix0 = vec4(float((aMix0.x >> 0u) & 15u), float((aMix0.x >> 4u) & 15u), float((aMix0.x >> 8u) & 15u), float((aMix0.x >> 12u) & 15u)) / 15.0;
    v_mix1 = vec4(float((aMix0.x >> 16u) & 15u), float((aMix0.x >> 20u) & 15u), float((aMix0.x >> 24u) & 15u), float((aMix0.x >> 28u) & 15u)) / 15.0;
    v_mix2 = vec4(float((aMix0.y >> 0u) & 15u), float((aMix0.y >> 4u) & 15u), float((aMix0.y >> 8u) & 15u), float((aMix0.y >> 12u) & 15u)) / 15.0;
    v_mix3 = vec4(float((aMix0.y >> 16u) & 15u), float((aMix0.y >> 20u) & 15u), float((aMix0.y >> 24u) & 15u), float((aMix0.y >> 28u) & 15u)) / 15.0;

    mat3 normalMat = mat3(transpose(inverse(actorModel)));
    v_worldNormal = normalize(normalMat * localRotation * aNormal);

    gl_Position = passProjection * passView * worldPos;
}
