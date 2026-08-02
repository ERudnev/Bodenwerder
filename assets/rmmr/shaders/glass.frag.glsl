#version 330 core

in vec2 v_uv0;
in float v_diffuse;
in float v_specular;

out vec4 FragColor;

uniform vec3 u_albedo;
uniform sampler2D u_albedoMap;

uniform vec3 u_ambientColor;
uniform float u_ambientIntensity;

uniform vec3 u_light0Color;
uniform float u_light0Intensity;

void main() {
    // debug06-style B&W opacity: luminance → alpha (white = denser glass).
    vec3 mask = texture(u_albedoMap, v_uv0).rgb;
    float alpha = dot(mask, vec3(1.0 / 3.0));
    if (alpha < 0.01) {
        discard;
    }

    float ambientGain = max(u_ambientIntensity, 0.0);
    float lightGain = max(u_light0Intensity, 0.0);

    vec3 ambient = u_albedo * u_ambientColor * (ambientGain / (1.0 + ambientGain));
    vec3 direct = u_albedo * u_light0Color * v_diffuse * (lightGain / (1.0 + lightGain));
    vec3 highlight = u_light0Color * v_specular * (lightGain / (1.0 + lightGain));

    FragColor = vec4(ambient + direct + highlight, alpha);
}
