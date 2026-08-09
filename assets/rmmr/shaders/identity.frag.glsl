#version 460 core

// GPU pick id (scenicAlias). Uniform for single-instance draws; geometry only needs positions.
uniform int u_scenicAlias;

layout (location = 0) out uint fragAlias;

void main() {
    fragAlias = uint(u_scenicAlias);
}
