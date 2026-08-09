#version 460 core

in vec2 v_uv0;
in float v_diffuse;
in float v_specular;
flat in uint v_drawId;

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

layout(binding = 0) uniform sampler2DArray u_albedoMap;

layout(std430, binding = 9) readonly buffer DrawMetadataBuffer {
    uvec2 metadata[];
};
layout(std430, binding = 10) readonly buffer SurfacePaletteBuffer {
    uint surfacePalette[];
};
layout(std430, binding = 11) readonly buffer PrimitiveSurfacesBuffer {
    uint primitiveSurfaces[];
};

void main() {
    // debug06-style B&W opacity: luminance → alpha (white = denser glass).
    uvec2 drawMetadata = metadata[v_drawId];
    uint surface = primitiveSurfaces[drawMetadata.x + uint(gl_PrimitiveID)];
    uint layer = surfacePalette[drawMetadata.y + surface];
    vec3 mask = texture(u_albedoMap, vec3(v_uv0, float(layer))).rgb;
    float alpha = dot(mask, vec3(1.0 / 3.0));
    if (alpha < 0.01) {
        discard;
    }

    float ambientGain = max(passAmbientColorIntensity.w, 0.0);
    float lightGain = max(passPrimaryLightPositionIntensity.w, 0.0);

    vec3 ambient = actorAlbedoOpacity.rgb * passAmbientColorIntensity.rgb * (ambientGain / (1.0 + ambientGain));
    vec3 direct = actorAlbedoOpacity.rgb * passPrimaryLightColorRange.rgb * v_diffuse * (lightGain / (1.0 + lightGain));
    vec3 highlight = passPrimaryLightColorRange.rgb * v_specular * (lightGain / (1.0 + lightGain));

    FragColor = vec4(ambient + direct + highlight, alpha * actorAlbedoOpacity.a);
}
