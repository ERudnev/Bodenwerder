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

float fetch_shadow(vec4 light_space_pos) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }

    float closest = texture(u_shadowMap, proj.xy).r;
    float current = proj.z - 0.005;
    return current > closest ? 0.35 : 1.0;
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
