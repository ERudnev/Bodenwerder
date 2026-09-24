#version 460 core
in vec3 vLocalPos;
out vec4 FragColor;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

void main() {
    vec2 coord = vLocalPos.xz * actorLatticePattern.y;
    vec2 fw = max(fwidth(coord) * 1.5, vec2(1e-6));
    vec2 cell = abs(fract(coord - 0.5) - 0.5);
    float line_x = 1.0 - smoothstep(0.0, fw.x, cell.x);
    float line_z = 1.0 - smoothstep(0.0, fw.y, cell.y);
    float intensity = max(line_x, line_z);

    float nx = round(coord.x);
    float nz = round(coord.y);
    float major_x = step(9.5, abs(nx)) * (1.0 - step(0.5, mod(abs(nx), 10.0)));
    float major_z = step(9.5, abs(nz)) * (1.0 - step(0.5, mod(abs(nz), 10.0)));
    float major = max(line_x * major_x, line_z * major_z);
    float major_factor = major / max(intensity, 1e-6);

    vec3 color = vec3(0.45, 0.48, 0.52) * intensity * actorAlbedoOpacity.a;
    float alpha = intensity * actorAlbedoOpacity.a;
    alpha *= mix(1.0, 1.5, clamp(major_factor, 0.0, 1.0));
    FragColor = vec4(color, alpha);
}
