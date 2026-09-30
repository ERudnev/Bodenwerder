#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
};

struct Instance {
    mat4 model;
    vec3 color;
    float radius;
};

layout(std430, binding = 8) readonly buffer InstanceBuffer {
    Instance instances[];
};

out vec3 v_worldNormal;
out vec3 v_color;

void main() {
    Instance inst = instances[gl_InstanceID];
    float radius = max(inst.radius, 1e-6);
    vec4 worldPos = inst.model * vec4(aPos * radius, 1.0);
    v_worldNormal = normalize(mat3(inst.model) * aNormal);
    v_color = inst.color;
    gl_Position = passProjection * passView * worldPos;
}
