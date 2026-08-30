#version 460 core

in vec3 v_worldPos;
in vec3 v_worldNormal;
in float v_heat;
in float v_fade;

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

vec3 blackbody(float kelvin) {
    float t = clamp((kelvin - 700.0) / 2500.0, 0.0, 1.0);
    vec3 ember = vec3(0.55, 0.04, 0.01);
    vec3 red = vec3(1.0, 0.14, 0.02);
    vec3 orange = vec3(1.0, 0.42, 0.06);
    vec3 yellow = vec3(1.0, 0.82, 0.38);
    vec3 white = vec3(1.0, 0.96, 0.90);
    if (t < 0.22)
        return mix(ember, red, t / 0.22);
    if (t < 0.45)
        return mix(red, orange, (t - 0.22) / 0.23);
    if (t < 0.72)
        return mix(orange, yellow, (t - 0.45) / 0.27);
    return mix(yellow, white, (t - 0.72) / 0.28);
}

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 cam = vec3(inverse(passView)[3]);
    vec3 V = normalize(cam - v_worldPos);
    float fres = pow(clamp(1.0 - abs(dot(N, V)), 0.0, 1.0), 2.2);
    float shell = mix(0.16, 1.0, fres);
    float kelvin = max(v_heat, 0.0);
    float glow = smoothstep(700.0, 2200.0, kelvin);
    vec3 hot = blackbody(kelvin);
    float fade = clamp(v_fade, 0.0, 1.0);
    vec3 rgb = hot * shell * fade * (1.4 + 3.2 * glow);
    FragColor = vec4(rgb, 1.0);
    BloomMask = shell * fade * (0.35 + 0.65 * glow);
}
