#version 460 core

in vec2 v_uv0;

out vec4 FragColor;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

layout(binding = 0) uniform sampler2D u_atlasTexture;

void main() {
    vec4 texel = texture(u_atlasTexture, v_uv0);
    if (texel.a < 0.01) {
        discard;
    }

    FragColor = vec4(texel.rgb * actorAlbedoOpacity.rgb, texel.a * actorAlbedoOpacity.a);
}
