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

void main() {
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(u_light0Pos - v_worldPos);

    float ndotl = max(dot(N, L), 0.0);

    float ambientGain = max(u_ambientIntensity, 0.0);
    float lightGain = max(u_light0Intensity, 0.0);

    vec3 ambient = u_albedo * u_ambientColor * (ambientGain / (1.0 + ambientGain));
    vec3 direct = u_albedo * u_light0Color * ndotl * (lightGain / (1.0 + lightGain));

    FragColor = vec4(ambient + direct, 1.0);
}

