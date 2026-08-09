#version 460 core

flat in uint v_scenicAlias;

layout (location = 0) out uint fragAlias;

void main() {
    fragAlias = v_scenicAlias;
}
