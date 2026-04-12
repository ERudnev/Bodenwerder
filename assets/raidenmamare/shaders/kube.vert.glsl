#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_worldPos;
out vec3 v_worldNormal;

void main() {
    vec4 worldPos = u_model * vec4(aPos, 1.0);
    v_worldPos = worldPos.xyz;

    mat3 normalMat = mat3(transpose(inverse(u_model)));
    v_worldNormal = normalize(normalMat * aNormal);

    gl_Position = u_projection * u_view * worldPos;
}
