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
    // Soft shell: bright limb of the expanding deformation front, faint face.
    float shell = pow(max(1.0 - chord, 0.0), 1.35);
    float face = chord * chord * 0.18;
    float density = clamp(shell + face, 0.0, 1.0);
    float fade = clamp(v_fade, 0.0, 1.0);
    float alpha = density * fade;
    FragColor = vec4(v_albedo, alpha);
    BloomMask = 0.0;
}
