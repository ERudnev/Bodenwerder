#include <eltanin/physics/clast.q1.h>

#include "physics/system.h"

#include <array>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>

namespace eltanin::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr std::size_t clastCount = 6;
        constexpr float axisEpsilon2 = 1.0e-12f;

        auto restOffset(float restRadius, std::size_t index) -> vec3 {
            switch (index) {
            case 0: return vec3{restRadius, 0.0f, 0.0f};
            case 1: return vec3{-restRadius, 0.0f, 0.0f};
            case 2: return vec3{0.0f, restRadius, 0.0f};
            case 3: return vec3{0.0f, -restRadius, 0.0f};
            case 4: return vec3{0.0f, 0.0f, restRadius};
            default: return vec3{0.0f, 0.0f, -restRadius};
            }
        }

        auto rejectAlong(vec3 candidate, vec3 axis) -> vec3 {
            return candidate - axis * glm::dot(candidate, axis);
        }

    } // namespace

    void Clast::Actions::satisfy(Stewarding context) {
        auto particles = context.direct<Particle>();
        auto clasts = context.direct<Clast>();
        for (auto [_, clast] : clasts.items) {
            if (clast.particles.size() != clastCount)
                continue;

            std::array<vec3, clastCount> currents;
            bool missing = false;
            vec3 com{0.0f, 0.0f, 0.0f};
            for (std::size_t index = 0; index < clastCount; ++index) {
                auto* particle = particles.items.find(clast.particles[index]);
                if (not particle) {
                    missing = true;
                    break;
                }
                currents[index] = particle->current;
                com += particle->current;
            }
            if (missing)
                continue;
            com /= static_cast<float>(clastCount);

            vec3 axisX = currents[0] - currents[1];
            const float lengthX2 = glm::dot(axisX, axisX);
            if (lengthX2 <= axisEpsilon2)
                continue;
            axisX /= std::sqrt(lengthX2);

            vec3 axisY = rejectAlong(currents[2] - currents[3], axisX);
            if (glm::dot(axisY, axisY) <= axisEpsilon2)
                axisY = rejectAlong(currents[4] - currents[5], axisX);
            const float lengthY2 = glm::dot(axisY, axisY);
            if (lengthY2 <= axisEpsilon2)
                continue;
            axisY /= std::sqrt(lengthY2);

            const vec3 axisZ = glm::cross(axisX, axisY);
            quat rotation = glm::quat_cast(mat3{axisX, axisY, axisZ});
            if (glm::dot(rotation, clast.restored.rotation) < 0.0f)
                rotation = -rotation;

            for (std::size_t index = 0; index < clastCount; ++index) {
                auto& particle = particles.items.at(clast.particles[index]);
                particle.current += Settings::constraintStiffness * (com + rotation * restOffset(clast.restRadius, index) - particle.current);
            }

            clast.restored = Pose{.position = com, .rotation = rotation};
        }
    }

}
