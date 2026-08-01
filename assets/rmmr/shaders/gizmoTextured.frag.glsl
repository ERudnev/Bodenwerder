#version 330 core

in vec2 v_uv0;

out vec4 FragColor;

uniform vec3 u_albedo;
uniform sampler2D u_albedoMap;
uniform float u_opacity;

void main() {
    vec4 texel = texture(u_albedoMap, v_uv0);
    FragColor = vec4(texel.rgb * u_albedo, texel.a * u_opacity);
}
