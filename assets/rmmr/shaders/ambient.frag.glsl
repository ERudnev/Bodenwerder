#version 330 core
out vec4 FragColor;

uniform vec3 u_albedo;
uniform vec3 u_ambientColor;
uniform float u_ambientIntensity;

void main() {
    float exposure = max(u_ambientIntensity, 0.0);
    vec3 lit = u_albedo * u_ambientColor * (exposure / (1.0 + exposure));
    FragColor = vec4(lit, 1.0);
}
