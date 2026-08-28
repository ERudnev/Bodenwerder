#version 460 core

in float v_heat;
in float v_bloom;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float BloomMask;

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
    float kelvin = max(v_heat, 0.0);
    float glow = smoothstep(800.0, 1800.0, kelvin);
    vec3 hot = blackbody(kelvin);
    FragColor = vec4(hot * glow * (1.1 + 2.2 * glow), 1.0);
    BloomMask = glow * v_bloom;
}
