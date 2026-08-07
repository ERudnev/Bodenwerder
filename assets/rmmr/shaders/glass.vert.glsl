#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv0;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

uniform vec3 u_light0Pos;

out vec2 v_uv0;
out float v_diffuse;
out float v_specular;

void main() {
    vec4 worldPos = u_model * vec4(aPos, 1.0);
    mat3 normalMat = mat3(transpose(inverse(u_model)));
    vec3 N = normalize(normalMat * aNormal);
    vec3 L = normalize(u_light0Pos - worldPos.xyz);
    vec3 eye = (inverse(u_view) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 V = normalize(eye - worldPos.xyz);
    vec3 H = normalize(L + V);

    v_uv0 = aUv0;
    v_diffuse = max(dot(N, L), 0.0);
    v_specular = pow(max(dot(N, H), 0.0), 64.0);

    gl_Position = u_projection * u_view * worldPos;
}
