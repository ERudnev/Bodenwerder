#version 460 core

layout(binding = 0) uniform sampler2D u_hdr;
layout(binding = 1) uniform sampler2D u_depth;
uniform mat4 u_invViewProj;
uniform vec3 u_cameraPos;
uniform vec3 u_fogColor;
uniform float u_fogDensity;
uniform float u_fogHeight;
uniform float u_fogHeightFalloff;
uniform float u_fogMaxOpacity;
uniform float u_fogDistanceScale;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

float opticalDepth(vec3 cameraPos, vec3 rayDir, float rayLength, float density, float height, float falloff, float distanceScale) {
    float densityAtCamera = density * exp(clamp(-falloff * (cameraPos.y - height), -80.0, 80.0));
    float kMu = falloff * rayDir.y;
    float integral = abs(kMu) < 1e-5 ? densityAtCamera * rayLength : densityAtCamera * (1.0 - exp(-clamp(kMu * rayLength, -80.0, 80.0))) / kMu;
    return distanceScale * max(integral, 0.0);
}

void main() {
    vec3 hdr = texture(u_hdr, vUv).rgb;
    float depth = texture(u_depth, vUv).r;
    vec4 worldH = u_invViewProj * vec4(vUv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec3 worldPos = worldH.xyz / worldH.w;
    vec3 delta = worldPos - u_cameraPos;
    float reconstructed = length(delta);
    vec3 rayDir = delta / max(reconstructed, 1e-6);
    float rayLength = depth >= 1.0 - 1e-5 ? 1.0e6 : reconstructed;
    float factor = min(u_fogMaxOpacity, 1.0 - exp(-min(opticalDepth(u_cameraPos, rayDir, rayLength, u_fogDensity, u_fogHeight, u_fogHeightFalloff, u_fogDistanceScale), 20.0)));
    fragColor = vec4(mix(hdr, u_fogColor, factor), 1.0);
}
