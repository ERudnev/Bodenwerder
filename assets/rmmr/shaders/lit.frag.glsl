#version 330 core

in vec3 v_worldPos;
in vec3 v_worldNormal;

out vec4 FragColor;

uniform vec3 u_albedo;

uniform vec3 u_ambientColor;
uniform float u_ambientIntensity;

uniform vec3 u_light0Pos;
uniform vec3 u_light0Color;
uniform float u_light0Intensity;

uniform mat4 u_lightSpaceMatrix;
uniform sampler2D u_shadowMap;

const float k_shadow_bias = 0.005;

float sample_shadow(vec2 uv, float current_depth) {
    float closest = texture(u_shadowMap, uv).r;
    return current_depth > closest ? 0.0 : 1.0;
}

float fetch_shadow(vec4 light_space_pos) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }

    float current_depth = proj.z - k_shadow_bias;
    vec2 texel = 1.0 / vec2(textureSize(u_shadowMap, 0));

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            shadow += sample_shadow(proj.xy + offset, current_depth);
        }
    }

    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(u_light0Pos - v_worldPos);

    float ndotl = max(dot(N, L), 0.0);
    float shadow = fetch_shadow(u_lightSpaceMatrix * vec4(v_worldPos, 1.0));

    float ambientGain = max(u_ambientIntensity, 0.0);
    float lightGain = max(u_light0Intensity, 0.0);

    vec3 ambient = u_albedo * u_ambientColor * (ambientGain / (1.0 + ambientGain));
    vec3 direct = u_albedo * u_light0Color * ndotl * shadow * (lightGain / (1.0 + lightGain));

    FragColor = vec4(ambient + direct, 1.0);
}
