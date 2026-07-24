#version 330 core

in vec2 v_uv0;

out vec4 FragColor;

uniform vec3 u_albedo;
uniform sampler2D u_atlasTexture;

void main() {
    vec4 texel = texture(u_atlasTexture, v_uv0);
    if (texel.a < 0.01) {
        discard;
    }

    FragColor = vec4(texel.rgb * u_albedo, texel.a);
}
