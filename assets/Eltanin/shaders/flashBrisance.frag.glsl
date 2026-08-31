#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in float v_heat;
in float v_fade;
in vec3 v_albedo;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
    vec4 passShutter;
};

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 cam = vec3(inverse(passView)[3]);
    vec3 V = normalize(cam - v_worldPos);
    float chord = clamp(abs(dot(N, V)), 0.0, 1.0);
    float density = chord * chord;
    float fade = clamp(v_fade, 0.0, 1.0);
    vec3 rgb = v_albedo * density * fade;
    FragColor = vec4(rgb, 1.0);
    BloomMask = 0.0;
}
