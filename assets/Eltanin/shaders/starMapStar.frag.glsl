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
    float mu = max(Nview.z, 0.0);
    float limb = 0.38 + 0.62 * mu;
    float rim = pow(1.0 - mu, 2.2);
    vec3 rgb = v_color * limb + v_color * 0.22 * rim;
    FragColor = vec4(rgb, 1.0);
    BloomMask = 0.22 + 0.40 * mu + 0.38 * rim;
}
