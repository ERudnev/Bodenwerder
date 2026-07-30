#version 330 core
in vec3 vWorldPos;
out vec4 FragColor;

uniform float u_patternScale;
uniform vec3 u_colorPrimary;
uniform vec3 u_colorSecondary;
uniform float u_opacity;

void main() {
    vec2 coord = vWorldPos.xz * u_patternScale;
    vec2 fw = max(fwidth(coord), vec2(1e-6));
    vec2 cell = abs(fract(coord - 0.5) - 0.5);
    float line_x = 1.0 - clamp(cell.x / fw.x, 0.0, 1.0);
    float line_z = 1.0 - clamp(cell.y / fw.y, 0.0, 1.0);
    float intensity = max(line_x, line_z);
    vec3 color = mix(u_colorSecondary, u_colorPrimary, intensity);

    // Axis accents on XZ ground: RGB ~ XYZ (Y not drawn on this plane).
    if (abs(vWorldPos.z) < 0.02) {
        color = vec3(1.0, 0.0, 0.0); // +X
    }
    if (abs(vWorldPos.x) < 0.02) {
        color = vec3(0.0, 0.0, 1.0); // +Z
    }

    // Every 10th line (not zero): mild alpha lift only.
    float nx = round(coord.x);
    float nz = round(coord.y);
    float major_x = step(9.5, abs(nx)) * (1.0 - step(0.5, mod(abs(nx), 10.0)));
    float major_z = step(9.5, abs(nz)) * (1.0 - step(0.5, mod(abs(nz), 10.0)));
    float major = max(line_x * major_x, line_z * major_z);
    float major_factor = major / max(intensity, 1e-6);

    float alpha = mix(0.2, 1.0, intensity) * u_opacity;
    alpha *= mix(1.0, 1.5, clamp(major_factor, 0.0, 1.0));
    FragColor = vec4(color, alpha);
}
