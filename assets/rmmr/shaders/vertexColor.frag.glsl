#version 460 core
in vec4 v_color0;

out vec4 FragColor;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

void main() {
    FragColor = vec4(actorAlbedoOpacity.rgb * v_color0.rgb, v_color0.a * actorAlbedoOpacity.a);
}
