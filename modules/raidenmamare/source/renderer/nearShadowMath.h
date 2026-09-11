#pragma once

#include <glm/glm.hpp>
#include <array>

namespace rmmr {

    struct NearShadowFrame {
        glm::mat4 lightSpace;
        glm::vec4 cameraRange;
    };

    inline auto nearShadowRays(const glm::mat4& camera) -> std::array<glm::vec3, 3> {
        const glm::vec3 forward = -glm::vec3{camera[2]};
        const glm::vec3 up{camera[1]};
        // A small forward-ground lead keeps the far rim inside the detailed
        // footprint; the other rays handle sky above the visible terrain.
        return {glm::normalize(forward + up * 0.15f), glm::normalize(forward), glm::normalize(forward - up * 0.5f)};
    }

    // Direction is transformed into mesh space without normalization, so the
    // slab parameter remains a world distance under non-uniform mesh scales.
    inline auto nearShadowRayHit(glm::vec3 origin, glm::vec3 direction, glm::vec3 lo, glm::vec3 hi, float limit) -> float {
        float entry = 0.0f;
        float leave = limit;
        for (int axis = 0; axis < 3; ++axis) {
            if (glm::abs(direction[axis]) < 1.0e-8f) {
                if (origin[axis] < lo[axis] or origin[axis] > hi[axis]) return limit;
                continue;
            }
            const float first = (lo[axis] - origin[axis]) / direction[axis];
            const float second = (hi[axis] - origin[axis]) / direction[axis];
            entry = glm::max(entry, glm::min(first, second));
            leave = glm::min(leave, glm::max(first, second));
            if (entry > leave) return limit;
        }
        return entry > 0.001f ? entry : leave;
    }

    inline auto nearShadowFrame(const glm::mat4& globalLight, const glm::mat4& camera, glm::vec3 focus, bool enabled, float halfExtent = 2000.0f, float resolution = 2048.0f) -> NearShadowFrame {
        // Do not fade out the focused ground simply because the camera rose.
        // Scale the receiver margin with this level's world-space footprint.
        const float reach = glm::max(3.0f * halfExtent, glm::distance(glm::vec3{camera[3]}, focus) + 2.0f * halfExtent);
        const glm::vec3 axisX = glm::normalize(glm::vec3{globalLight[0].x, globalLight[1].x, globalLight[2].x});
        const glm::vec3 axisY = glm::normalize(glm::vec3{globalLight[0].y, globalLight[1].y, globalLight[2].y});
        const float texel = 2.0f * halfExtent / resolution;
        // Fixed world-space scale and snapped origin prevent sub-texel swimming.
        // Keep the global Z interval: distant upstream casters must not be clipped.
        auto matrix = globalLight;
        for (int column = 0; column < 3; ++column) {
            matrix[column].x = axisX[column] / halfExtent;
            matrix[column].y = axisY[column] / halfExtent;
        }
        matrix[3].x = -glm::round(glm::dot(axisX, focus) / texel) * texel / halfExtent;
        matrix[3].y = -glm::round(glm::dot(axisY, focus) / texel) * texel / halfExtent;
        return NearShadowFrame{matrix, glm::vec4{glm::vec3{camera[3]}, enabled ? reach : 0.0f}};
    }

}
