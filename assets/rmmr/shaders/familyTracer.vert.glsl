#version 460 core

layout (location = 0) in vec3 aPos;

layout(std140, binding = 0) uniform PassStateBuffer {
    mat4 passView;
    mat4 passProjection;
    mat4 passLightSpace;
    vec4 passAmbientColorIntensity;
    vec4 passPrimaryLightPositionIntensity;
    vec4 passPrimaryLightColorRange;
    vec4 passShutter;
};

struct Instance {
    mat4 model;
    float speed;
    float heat;
    vec2 pad;
};

layout(std430, binding = 8) readonly buffer InstanceBuffer {
    Instance instances[];
};

out float v_heat;
out float v_bloom;

void main() {
    Instance inst = instances[gl_InstanceID];
    const float restLength = 0.20;
    const float caliber = 0.015;
    const float minNdc = 0.0009;
    const float maxNdc = 0.018;
    float smear = max(restLength, inst.speed * passShutter.x);
    float zScale = smear / restLength;
    vec4 viewCenter = passView * inst.model * vec4(0.0, 0.0, 0.0, 1.0);
    float viewZ = max(-viewCenter.z, 0.05);
    float ndcR = caliber * abs(passProjection[1][1]) / viewZ;
    float floorMix = smoothstep(20.0, 110.0, viewZ);
    float targetNdc = mix(ndcR, max(ndcR, minNdc), floorMix);
    targetNdc = min(targetNdc, maxNdc);
    float xyScale = targetNdc / max(ndcR, 1e-6);
    vec3 pos = aPos;
    pos.z *= zScale;
    pos.xy *= xyScale;
    gl_Position = passProjection * passView * inst.model * vec4(pos, 1.0);
    v_heat = inst.heat;
    v_bloom = 1.0 - smoothstep(20.0, 90.0, viewZ);
}
