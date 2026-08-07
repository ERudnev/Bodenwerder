#version 450 core
in vec4 v_color0;

out vec4 FragColor;

uniform vec3 u_albedo;
uniform float u_opacity;

void main() {
    FragColor = vec4(u_albedo * v_color0.rgb, v_color0.a * u_opacity);
}
