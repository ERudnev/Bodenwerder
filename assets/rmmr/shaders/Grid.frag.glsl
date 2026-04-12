#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;

uniform float u_patternScale;
uniform vec3 u_colorPrimary;
uniform vec3 u_colorSecondary;

void main() {
    vec2 coord = vWorldPos.xz * u_patternScale;
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);
    float intensity = 1.0 - clamp(line, 0.0, 1.0);
    vec3 color = mix(u_colorSecondary, u_colorPrimary, intensity);

    if (abs(vWorldPos.x) < 0.02) {
        color = vec3(1.0, 0.0, 0.0);
    }
    if (abs(vWorldPos.z) < 0.02) {
        color = vec3(0.0, 0.0, 1.0);
    }

    FragColor = vec4(color, 1.0);
}
