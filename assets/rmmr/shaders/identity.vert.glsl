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

flat out uint v_scenicAlias;

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
    vec3 localPosition = orientationMatrix(pose.w) * aPos + vec3(pose.xyz) * actorLatticePattern.x;
    gl_Position = passProjection * passView * actorModel * vec4(localPosition, 1.0);
    v_scenicAlias = actorScenicAlias;
}
