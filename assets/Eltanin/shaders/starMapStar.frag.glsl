#version 460 core

in vec3 v_worldNormal;
in vec3 v_color;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
};

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 Nview = normalize(mat3(passView) * N);
    float wrap = 0.28 + 0.72 * max(dot(Nview, vec3(0.18, 0.42, 0.90)), 0.0);
    vec3 rgb = v_color * (0.55 + 1.35 * wrap);
    FragColor = vec4(rgb, 1.0);
    BloomMask = 0.35 + 0.45 * wrap;
}
