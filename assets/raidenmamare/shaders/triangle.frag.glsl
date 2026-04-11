#version 330 core
out vec4 FragColor;

uniform vec3 u_albedo;
uniform vec3 u_lightColor;
uniform float u_lightIntensity;

void main() {
    float exposure = max(u_lightIntensity, 0.0);
    vec3 lit = u_albedo * u_lightColor * (exposure / (1.0 + exposure));
    FragColor = vec4(lit, 1.0);
}
