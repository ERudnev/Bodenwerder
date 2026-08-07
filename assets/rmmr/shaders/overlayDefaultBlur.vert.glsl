#version 450 core

// Fullscreen triangle via gl_VertexID (no VBO).
out vec2 vUv;

void main() {
    const vec2 pos[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    vec2 p = pos[gl_VertexID];
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
