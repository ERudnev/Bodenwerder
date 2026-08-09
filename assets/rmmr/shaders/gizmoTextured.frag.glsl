#version 450 core

in vec2 v_uv0;

out vec4 FragColor;

uniform vec3 u_albedo;
layout(binding = 0) uniform sampler2DArray u_albedoMap;
uniform int u_albedoLayer;
uniform float u_opacity;

void main() {
    vec4 texel = texture(u_albedoMap, vec3(v_uv0, float(u_albedoLayer)));
    FragColor = vec4(texel.rgb * u_albedo, texel.a * u_opacity);
}
