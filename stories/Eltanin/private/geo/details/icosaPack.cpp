#include "geo/details/icosaPack.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    namespace {

        struct Face {
            integer diamond;
            bool lower;
            integer a;
            integer b;
            integer c;
        };

        struct Topology {
            std::array<vec3, IcosaPack::shellCount> vertices;
            std::array<IcosaPack::Diamond, IcosaPack::diamondCount> diamonds;
            std::array<Face, IcosaPack::faceCount> faces;
            std::array<vec3, IcosaPack::faceCount> normals;
        };

        auto ringVertex(float latitude, float longitude) -> vec3 {
            const float ring = std::cos(latitude);
            return vec3{std::cos(longitude) * ring, std::sin(latitude), std::sin(longitude) * ring};
        }

        auto onDiamond(float u, float v, vec3 top, vec3 right, vec3 bottom, vec3 left) -> vec3 {
            if (u + v <= 1.0f)
                return (1.0f - u - v) * top + u * right + v * left;
            return (u + v - 1.0f) * bottom + (1.0f - v) * right + (1.0f - u) * left;
        }

        auto barycentric(vec3 dir, vec3 a, vec3 b, vec3 c, vec3 normal) -> vec3 {
            const vec3 point = dir * (glm::dot(a, normal) / glm::dot(dir, normal));
            const vec3 edgeAB = b - a;
            const vec3 edgeAC = c - a;
            const vec3 fromA = point - a;
            const float d00 = glm::dot(edgeAB, edgeAB);
            const float d01 = glm::dot(edgeAB, edgeAC);
            const float d11 = glm::dot(edgeAC, edgeAC);
            const float d20 = glm::dot(fromA, edgeAB);
            const float d21 = glm::dot(fromA, edgeAC);
            const float denom = d00 * d11 - d01 * d01;
            const float baryB = (d11 * d20 - d01 * d21) / denom;
            const float baryC = (d00 * d21 - d01 * d20) / denom;
            return vec3{1.0f - baryB - baryC, baryB, baryC};
        }

        auto makeTopology() -> Topology {
            Topology topology{};
            const float pi = std::numbers::pi_v<float>;
            const float latitude = std::atan(0.5f);
            topology.vertices[0] = vec3{0.0f, 1.0f, 0.0f};
            topology.vertices[11] = vec3{0.0f, -1.0f, 0.0f};
            for (integer spoke = 0; spoke < 5; ++spoke) {
                const float turn = static_cast<float>(spoke) * (2.0f * pi / 5.0f);
                topology.vertices[1 + spoke] = ringVertex(latitude, turn);
                topology.vertices[6 + spoke] = ringVertex(-latitude, turn + pi / 5.0f);
            }

            auto fixWinding = [&](IcosaPack::Diamond diamond) -> IcosaPack::Diamond {
                const vec3 top = topology.vertices[diamond.top];
                const vec3 right = topology.vertices[diamond.right];
                const vec3 left = topology.vertices[diamond.left];
                if (glm::dot(glm::cross(right - top, left - top), top) < 0.0f)
                    return IcosaPack::Diamond{.top = diamond.top, .right = diamond.left, .bottom = diamond.bottom, .left = diamond.right};
                return diamond;
            };

            for (integer spoke = 0; spoke < 5; ++spoke) {
                const integer next = (spoke + 1) % 5;
                topology.diamonds[spoke] = fixWinding(IcosaPack::Diamond{.top = 0, .right = 1 + next, .bottom = 6 + spoke, .left = 1 + spoke});
                topology.diamonds[5 + spoke] = fixWinding(IcosaPack::Diamond{.top = 1 + next, .right = 6 + next, .bottom = 11, .left = 6 + spoke});
            }

            for (integer diamond = 0; diamond < IcosaPack::diamondCount; ++diamond) {
                const auto corners = topology.diamonds[diamond];
                topology.faces[diamond] = Face{.diamond = diamond, .lower = false, .a = corners.top, .b = corners.right, .c = corners.left};
                topology.faces[diamond + IcosaPack::diamondCount] = Face{.diamond = diamond, .lower = true, .a = corners.right, .b = corners.bottom, .c = corners.left};
            }

            for (integer face = 0; face < IcosaPack::faceCount; ++face) {
                const auto& triangle = topology.faces[face];
                const vec3 a = topology.vertices[triangle.a];
                const vec3 b = topology.vertices[triangle.b];
                const vec3 c = topology.vertices[triangle.c];
                vec3 normal = glm::normalize(glm::cross(b - a, c - a));
                if (glm::dot(normal, a + b + c) < 0.0f)
                    normal = -normal;
                topology.normals[face] = normal;
            }
            return topology;
        }

        auto topology() -> const Topology& {
            static const Topology built = makeTopology();
            return built;
        }

    } // namespace

    auto IcosaPack::vertices() -> const std::array<vec3, shellCount>& {
        return topology().vertices;
    }

    auto IcosaPack::diamonds() -> const std::array<Diamond, diamondCount>& {
        return topology().diamonds;
    }

    auto IcosaPack::locate(vec3 direction) -> Sample {
        const vec3 dir = glm::normalize(direction);
        const auto& built = topology();
        integer bestFace = 0;
        float bestDot = glm::dot(dir, built.normals[0]);
        for (integer face = 1; face < faceCount; ++face) {
            const float facing = glm::dot(dir, built.normals[face]);
            if (facing > bestDot) {
                bestDot = facing;
                bestFace = face;
            }
        }
        const Face& triangle = built.faces[bestFace];
        const vec3 bary = barycentric(dir, built.vertices[triangle.a], built.vertices[triangle.b], built.vertices[triangle.c], built.normals[bestFace]);
        if (not triangle.lower)
            return Sample{.diamond = triangle.diamond, .u = bary.y, .v = bary.z};
        return Sample{.diamond = triangle.diamond, .u = 1.0f - bary.z, .v = 1.0f - bary.x};
    }

    auto IcosaPack::direction(Sample sample) -> vec3 {
        const auto& built = topology();
        const Diamond& diamond = built.diamonds[sample.diamond];
        return glm::normalize(onDiamond(sample.u, sample.v, built.vertices[diamond.top], built.vertices[diamond.right], built.vertices[diamond.bottom], built.vertices[diamond.left]));
    }

}
