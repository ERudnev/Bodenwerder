#version 330 core

// Engine defaultBlur: box-blur scene color using only samples whose identiffy != 0.
uniform sampler2D u_sceneColor;
uniform usampler2D u_identiffyMap;
uniform vec2 u_texelSize;

in vec2 vUv;
layout (location = 0) out vec4 fragColor;

void main() {
    const int R = 5;
    uint centerId = texture(u_identiffyMap, vUv).r;
    if (centerId == 0u) {
        fragColor = vec4(0.0);
        return;
    }

    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int y = -R; y <= R; ++y) {
        for (int x = -R; x <= R; ++x) {
            vec2 uv = vUv + vec2(float(x), float(y)) * u_texelSize;
            if (texture(u_identiffyMap, uv).r == 0u)
                continue;
            acc += texture(u_sceneColor, uv).rgb;
            wsum += 1.0;
        }
    }
    if (wsum < 1e-5) {
        fragColor = vec4(0.0);
        return;
    }
    fragColor = vec4(acc / wsum, 0.65);
}
