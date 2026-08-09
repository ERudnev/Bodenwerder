#version 460 core

in vec2 v_uv0;
in vec4 v_color0;
flat in uint v_drawId;
out vec4 FragColor;

uniform vec3 u_albedo;
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
    uvec2 drawMetadata = metadata[v_drawId];
    uint surface = primitiveSurfaces[drawMetadata.x + uint(gl_PrimitiveID)];
    uint layer = surfacePalette[drawMetadata.y + surface];
    vec4 texel = texture(u_albedoMap, vec3(v_uv0, float(layer)));
    vec3 rgb = texel.rgb * u_albedo * v_color0.rgb * texel.a;
    if (dot(rgb, rgb) < 1e-8) {
        discard;
    }
    FragColor = vec4(rgb, 1.0);
}
