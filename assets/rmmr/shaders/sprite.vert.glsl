#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv0;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

layout(std430, binding = 0) readonly buffer AtlasEntries {
    ivec4 data[];
};

uniform int u_spriteIndex;
uniform vec2 u_inverseAtlasSize;

out vec2 v_uv0;

void main() {
    ivec4 rect = data[u_spriteIndex * 2];
    ivec4 extra = data[u_spriteIndex * 2 + 1];

    vec2 spriteMin = vec2(rect.xy);
    vec2 spriteSize = vec2(rect.zw);
    vec2 pivotDown = vec2(extra.xy) - spriteMin;
    vec2 pivot = vec2(pivotDown.x, spriteSize.y - pivotDown.y);
    vec2 localPixels = aUv0 * spriteSize - pivot;

    vec4 worldPos = u_model * vec4(localPixels, aPos.z, 1.0);
    gl_Position = u_projection * u_view * worldPos;

    vec2 uvMin = spriteMin * u_inverseAtlasSize;
    vec2 uvMax = (spriteMin + spriteSize) * u_inverseAtlasSize;
    v_uv0 = vec2(
        mix(uvMin.x, uvMax.x, aUv0.x),
        mix(uvMin.y, uvMax.y, aUv0.y));
}
