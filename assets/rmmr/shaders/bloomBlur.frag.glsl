#version 460 core

layout(binding = 0) uniform sampler2D u_src;
uniform vec2 u_texelDir;
uniform float u_radius;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

void main() {
    const int kMax = 8;
    float sigma = max(u_radius / 3.0, 0.01);
    float twoSigma2 = 2.0 * sigma * sigma;
    int radius = int(ceil(u_radius));
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = -kMax; i <= kMax; ++i) {
        if (abs(i) > radius) continue;
        float fi = float(i);
        float w = exp(-(fi * fi) / twoSigma2);
        acc += texture(u_src, vUv + u_texelDir * fi).rgb * w;
        wsum += w;
    }
    fragColor = vec4(acc / max(wsum, 1e-6), 1.0);
}
