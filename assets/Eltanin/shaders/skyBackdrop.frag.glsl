#version 460 core

in vec3 v_dir;
out vec4 FragColor;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));
    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    return mix(mix(nx00, nx10, f.y), mix(nx01, nx11, f.y), f.z);
}

float faintField(vec3 dir, float cells, float keep) {
    vec3 p = dir * cells;
    vec3 cell = floor(p);
    vec3 f = fract(p) - 0.5;
    float h = hash13(cell);
    if (h < keep)
        return 0.0;
    float spark = exp(-dot(f, f) * 28.0);
    return spark * ((h - keep) / max(1.0 - keep, 1e-4));
}

void main() {
    vec3 dir = normalize(v_dir);
    float disk = exp(-dir.y * dir.y * 14.0);
    float core = exp(-dot(dir - vec3(0.0, 0.0, -1.0), dir - vec3(0.0, 0.0, -1.0)) * 1.6);
    float grain = valueNoise(dir * 7.0) * 0.55 + valueNoise(dir.zxy * 19.0 + 3.1) * 0.45;
    float unresolved = faintField(dir, 95.0, 0.973) * 0.55 + faintField(dir, 210.0, 0.988) * 0.45;

    vec3 rgb = vec3(0.011, 0.013, 0.020) * (0.70 + 0.30 * grain);
    rgb += vec3(0.038, 0.026, 0.016) * disk;
    rgb += vec3(0.022, 0.018, 0.014) * core;
    rgb += vec3(0.70, 0.78, 1.0) * unresolved * 0.09;
    FragColor = vec4(rgb, 1.0);
}
