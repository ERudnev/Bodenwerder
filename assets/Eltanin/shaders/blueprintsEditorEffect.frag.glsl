#version 330 core

// Blueprints selection overlay (Tesla-style):
// (1) segmentation — foreign/own in selectedMap (smaller radius)
// (2) outer boundary of the selection set — zeros vs non-zeros
// (3) 2px diagonal stripes — occluded selected only (frontId != centerSel)
// (4) visible selected fill — absent
// Hover: same outline idea on identiffyMap, no fill

uniform usampler2D u_identiffyMap;
uniform usampler2D u_selectedMap;
uniform uint u_under;

in vec2 vUv;
layout (location = 0) out vec4 fragColor;

const int R_OUTLINE = 4;
const int R_SEG = 3; // ~outline / 1.5

uint fetchId(usampler2D map, ivec2 p, ivec2 size) {
    p = clamp(p, ivec2(0), size - ivec2(1));
    return texelFetch(map, p, 0).r;
}

float edgeWeight(float ratio) {
    return smoothstep(0.08, 0.38, ratio);
}

void main() {
    ivec2 size = textureSize(u_selectedMap, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);

    uint frontId = fetchId(u_identiffyMap, pixel, size);
    uint centerSel = fetchId(u_selectedMap, pixel, size);

    vec4 color = vec4(0.0);

    // Hover frame on the frontmost object under cursor — outline only, no fill.
    if (u_under != 0u && frontId == u_under) {
        int hOwn = 0;
        int hForeign = 0;
        for (int y = -R_OUTLINE; y <= R_OUTLINE; ++y) {
            for (int x = -R_OUTLINE; x <= R_OUTLINE; ++x) {
                uint s = fetchId(u_identiffyMap, pixel + ivec2(x, y), size);
                if (s == u_under)
                    ++hOwn;
                else
                    ++hForeign;
            }
        }
        float hoverEdge = float(hForeign) / float(max(hOwn + hForeign, 1));
        float hoverContrib = edgeWeight(hoverEdge);
        color += vec4(1.0, 0.25, 0.15, 0.72) * hoverContrib;
    }

    if (centerSel == 0u) {
        fragColor = color;
        return;
    }

    int ownSeg = 0;
    int foreignSeg = 0;
    int zeros = 0;
    int nonzero = 0;

    for (int y = -R_OUTLINE; y <= R_OUTLINE; ++y) {
        for (int x = -R_OUTLINE; x <= R_OUTLINE; ++x) {
            uint s = fetchId(u_selectedMap, pixel + ivec2(x, y), size);
            if (s == 0u)
                ++zeros;
            else
                ++nonzero;

            if (abs(x) <= R_SEG && abs(y) <= R_SEG) {
                if (s == centerSel)
                    ++ownSeg;
                else
                    ++foreignSeg;
            }
        }
    }

    float segmentation = float(foreignSeg) / float(max(ownSeg + foreignSeg, 1));
    float outline = float(zeros) / float(max(zeros + nonzero, 1));

    // (3) occluded only: another Identified won the shared depth / all-ID.
    // Visible selected has frontId == centerSel (needs GL_LEQUAL on identity pass).
    float stripes = 0.0;
    if (frontId != 0u && frontId != centerSel) {
        float m = mod(gl_FragCoord.x + gl_FragCoord.y, 8.0);
        stripes = step(m, 2.0);
    }

    float segEdge = smoothstep(0.12, 0.42, segmentation);
    float outEdge = edgeWeight(outline);

    float contrib = 0.0;
    contrib += segEdge * 0.95;
    contrib += outEdge * 0.85;
    contrib += stripes * 0.55;
    contrib = clamp(contrib, 0.0, 1.0);

    color += vec4(0.15, 1.0, 0.42, 0.72) * contrib;
    fragColor = color;
}
