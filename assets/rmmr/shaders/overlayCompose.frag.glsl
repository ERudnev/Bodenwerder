#version 460 core

layout(binding = 0) uniform sampler2D u_overlay;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(u_overlay, vUv);
}
