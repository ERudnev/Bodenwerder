#version 450 core

in vec2 v_uv0;
in vec4 v_color0;
out vec4 FragColor;

uniform vec3 u_albedo;
layout(binding = 0) uniform sampler2D u_albedoMap;

void main() {
    vec4 texel = texture(u_albedoMap, v_uv0);
    vec3 rgb = texel.rgb * u_albedo * v_color0.rgb * texel.a;
    if (dot(rgb, rgb) < 1e-8) {
        discard;
    }
    FragColor = vec4(rgb, 1.0);
}
