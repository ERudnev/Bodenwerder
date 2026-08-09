#version 460 core
out vec4 FragColor;

layout(std430, binding = 7) readonly buffer ActorStateBuffer {
    mat4 actorModel;
    vec4 actorAlbedoOpacity;
    vec2 actorLatticePattern;
    uint actorScenicAlias;
    uint actorSpriteIndex;
};

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
};

void main() {
    float exposure = max(passAmbientColorIntensity.w, 0.0);
    vec3 lit = actorAlbedoOpacity.rgb * passAmbientColorIntensity.rgb * (exposure / (1.0 + exposure));
    FragColor = vec4(lit, 1.0);
}
