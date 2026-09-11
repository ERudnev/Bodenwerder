#version 460 core

layout(binding = 0) uniform sampler2D u_hdr;
layout(binding = 1) uniform sampler2D u_bloom;
uniform float u_intensity;
// One stop below the previous exposure; keep the display transfer intact.
const float exposure = 0.5;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

vec3 linearToSrgb(vec3 linearColor) {
    // The window pass does not enable GL_FRAMEBUFFER_SRGB. Encode once here,
    // after HDR lighting, bloom and ACES, before display-space dithering.
    vec3 low = linearColor * 12.92;
    vec3 high = 1.055 * pow(linearColor, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, greaterThan(linearColor, vec3(0.0031308)));
}

void main() {
    vec3 hdr = texture(u_hdr, vUv).rgb;
    vec3 bloom = texture(u_bloom, vUv).rgb;
    vec3 ldr = linearToSrgb(aces((hdr + bloom * u_intensity) * exposure));
    float n = ign(gl_FragCoord.xy);
    float tri = n < 0.5 ? sqrt(2.0 * n) - 1.0 : 1.0 - sqrt(2.0 - 2.0 * n);
    fragColor = vec4(clamp(ldr + tri / 255.0, 0.0, 1.0), 1.0);
}
