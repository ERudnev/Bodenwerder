#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv0;

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

out vec2 v_uv0;
out float v_diffuse;
out float v_specular;
flat out uint v_drawId;

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
    mat3 normalMat = mat3(transpose(inverse(actorModel)));
    vec3 N = normalize(normalMat * localRotation * aNormal);
    vec3 L = normalize(passPrimaryLightPositionIntensity.xyz - worldPos.xyz * float(passPrimaryLightColorRange.w > 0.0));
    vec3 eye = (inverse(passView) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 V = normalize(eye - worldPos.xyz);
    vec3 H = normalize(L + V);

    v_uv0 = aUv0;
    v_diffuse = max(dot(N, L), 0.0);
    v_specular = pow(max(dot(N, H), 0.0), 64.0);
    v_drawId = uint(gl_DrawID);

    gl_Position = passProjection * passView * worldPos;
}
