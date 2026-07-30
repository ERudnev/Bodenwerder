#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv0;
layout (location = 2) in vec4 aColor0;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec2 v_uv0;
out vec4 v_color0;

void main() {
    v_uv0 = aUv0;
    v_color0 = aColor0;
    gl_Position = u_projection * u_view * u_model * vec4(aPos, 1.0);
}
