#version 460 core

layout(binding = 0) uniform sampler2D u_hdr;
layout(binding = 1) uniform sampler2D u_bloomMask;
layout(location = 0) out vec4 outBloom;

void main() {
    const int scale = 4;
    ivec2 dst = ivec2(gl_FragCoord.xy);
    ivec2 srcSize = textureSize(u_hdr, 0);
    ivec2 srcBase = dst * scale;
    vec3 acc = vec3(0.0);
    int count = 0;
    for (int y = 0; y < scale; ++y) {
        for (int x = 0; x < scale; ++x) {
            ivec2 src = min(srcBase + ivec2(x, y), srcSize - ivec2(1));
            acc += texelFetch(u_hdr, src, 0).rgb * texelFetch(u_bloomMask, src, 0).r;
            ++count;
        }
    }
    outBloom = vec4(acc / float(max(count, 1)), 1.0);
}
