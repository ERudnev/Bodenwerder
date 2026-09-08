#include <eltanin/resources/geometry.q1.h>

#include "galaxy.h"

#include <base/logging.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/semantics/geometry.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::resource {

    using namespace fqsm::api;
    using namespace rmmr;
    using rmmr::resource::builders::geometry::CpuPresentation;

    namespace {

        constexpr float k_mesh_radius = 100.0f;
        constexpr float polarAngularDeg = 2.5f; // ~5× old 0.5°; Lorentz core ≈ 10–15 px at FOV 100 / 5k (half-moon)
        constexpr float polarRadius = k_mesh_radius * 0.98f;
        // Sun: R≈8.2 kpc, z≈+20 pc (in the mid-plane — not lifted above the disk).
        constexpr float k_kpc_ly = 3261.56f;
        constexpr float k_pc_ly = 3.26156f;
        constexpr glm::vec3 k_observer_ly{8.2f * k_kpc_ly, 20.0f * k_pc_ly, 0.0f};

        struct BillboardUv {
            float u0;
            float v0;
            float u1;
            float v1;
        };

        struct PendingBillboard {
            glm::vec3 direction;
            float radius;
            float half;
            vec4 color;
            BillboardUv uv;
        };

        auto quad_size_from_angular_diameter(float mesh_radius, float angular_diameter_deg) -> float {
            const float degrees = std::max(angular_diameter_deg, 0.05f);
            const float theta = degrees * std::numbers::pi_v<float> / 180.0f;
            return 2.0f * mesh_radius * std::tan(0.5f * theta);
        }

        auto unitHash(vec3 p) -> float {
            vec3 q = glm::fract(p * vec3{0.0143f, 0.0271f, 0.0097f});
            q += glm::dot(q, vec3{q.y, q.z, q.x} + 33.33f);
            return glm::fract((q.x + q.y) * q.z);
        }

        auto fieldTemperature(vec3 positionLy) -> float {
            const float warm = std::pow(unitHash(positionLy), 0.70f);
            return 2900.0f + warm * 3600.0f;
        }

        auto starTint(float temperatureK) -> vec3 {
            const float t = std::clamp((temperatureK - 2500.0f) / 9500.0f, 0.0f, 1.0f);
            const vec3 amber{1.00f, 0.56f, 0.30f};
            const vec3 peach{1.00f, 0.84f, 0.66f};
            const vec3 ice{0.70f, 0.78f, 1.00f};
            return t < 0.40f ? glm::mix(amber, peach, t / 0.40f) : glm::mix(peach, ice, (t - 0.40f) / 0.60f);
        }

        auto tintedBrightness(vec3 tint, float brightness) -> vec3 {
            const float luma = glm::dot(tint, vec3{0.2126f, 0.7152f, 0.0722f});
            const float near = std::clamp((brightness - 0.32f) / 2.68f, 0.0f, 1.0f);
            const float punch = 1.15f + 1.25f * near;
            return glm::max(vec3{0.0f, 0.0f, 0.0f}, vec3{luma} + (tint - vec3{luma}) * punch) * brightness;
        }

        auto direction_and_distance(const glm::vec3& position_ly) -> std::pair<glm::vec3, float> {
            const glm::vec3 offset = position_ly - k_observer_ly;
            const float distance = glm::length(offset);
            if (distance < 1.0f) {
                return {glm::vec3{0.0f, 1.0f, 0.0f}, 1.0f};
            }
            const glm::vec3 galactic = offset / distance;
            // Galaxy: Sol at +X, GC at origin. Local frame looks at GC along −Z: (x,y,z) → (z,y,x).
            return {glm::vec3{galactic.z, galactic.y, galactic.x}, distance};
        }

        void emit_billboard(CpuPresentation& cpu, const PendingBillboard& billboard) {
            const vec3 center = vec3{billboard.direction} * billboard.radius;
            const vec3 up = std::abs(billboard.direction.y) < 0.99f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f};
            const vec3 tangent = glm::normalize(glm::cross(up, billboard.direction));
            const vec3 bitangent = glm::cross(billboard.direction, tangent);

            const float half = billboard.half;
            const vec3 p00 = center - tangent * half - bitangent * half;
            const vec3 p10 = center + tangent * half - bitangent * half;
            const vec3 p11 = center + tangent * half + bitangent * half;
            const vec3 p01 = center - tangent * half + bitangent * half;

            const Pos corners[6]{p00, p01, p11, p00, p11, p10};
            const UV uvs[6]{
                UV{billboard.uv.u0, billboard.uv.v0}, UV{billboard.uv.u0, billboard.uv.v1}, UV{billboard.uv.u1, billboard.uv.v1},
                UV{billboard.uv.u0, billboard.uv.v0}, UV{billboard.uv.u1, billboard.uv.v1}, UV{billboard.uv.u1, billboard.uv.v0},
            };
            for (int corner = 0; corner < 6; ++corner) {
                cpu.positions.push_back(corners[corner]);
                cpu.uv0.push_back(uvs[corner]);
                cpu.color0.push_back(billboard.color);
            }
        }

        auto build_cpu(const SkySphereGenerator::Quantum& quantum) -> CpuPresentation {
            const std::size_t star_count = quantum.count > 0 ? static_cast<std::size_t>(quantum.count) : std::size_t{0};
            const Galaxy galaxy = generate_spiral_galaxy(star_count, static_cast<std::uint32_t>(quantum.seed));

            const BillboardUv starUv{0.0f, 0.0f, 1.0f, 1.0f};
            const float star_half = 0.5f * quad_size_from_angular_diameter(k_mesh_radius, quantum.angular_diameter_deg);
            const float polarHalf = 0.5f * quad_size_from_angular_diameter(polarRadius, polarAngularDeg);

            CpuPresentation cpu{
                .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "uv0", "color0"}),
                .positions = {},
                .normals = {},
                .uv0 = {},
                .color0 = {},
                .indices = {},
            };
            cpu.positions.reserve((galaxy.size() + 6) * 6);
            cpu.uv0.reserve((galaxy.size() + 6) * 6);
            cpu.color0.reserve((galaxy.size() + 6) * 6);

            for (const Star& star : galaxy) {
                const auto [direction, distance] = direction_and_distance(star.position_ly);
                constexpr float distancePower = 1.15f;
                constexpr float distanceRef = 4000.0f;
                constexpr float sizeFloor = 0.55f;
                const float relative = std::max(star.luminosity_sun, 1.0e-8f) * std::pow(distanceRef / std::max(distance, 25.0f), distancePower);
                const float sizeScale = std::clamp(sizeFloor * std::pow(std::max(relative, 1.0f), 0.005f), sizeFloor, sizeFloor * 2.8f);
                const float brightness = std::clamp(0.85f * std::pow(std::max(relative, 1.0e-4f), 0.30f), 0.32f, 3.0f);
                const float near = std::clamp((brightness - 0.32f) / 2.68f, 0.0f, 1.0f);
                const float visualK = glm::mix(star.temperature_K, fieldTemperature(star.position_ly), near);
                const vec3 rgb = tintedBrightness(starTint(visualK), brightness);
                emit_billboard(cpu, PendingBillboard{
                    .direction = direction,
                    .radius = k_mesh_radius,
                    .half = star_half * sizeScale,
                    .color = vec4{rgb, 1.0f},
                    .uv = starUv,
                });
            }

            const struct { vec3 direction; vec3 rgb; } polars[]{
                {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
                {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}},
                {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                {{0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
                {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 0.0f}},
            };
            for (const auto& polar : polars) {
                emit_billboard(cpu, PendingBillboard{
                    .direction = polar.direction,
                    .radius = polarRadius,
                    .half = polarHalf,
                    .color = vec4{polar.rgb * 2.4f, 1.0f},
                    .uv = starUv,
                });
            }

            base::message("eltanin::SkySphereGenerator: stars={} polars=6", galaxy.size());
            return cpu;
        }

    } // namespace

    auto SkySphereGenerator::Actions::materialize(Writing context, Id asset_id, rmmr::system::Device::Id device)
        -> optional<rmmr::resource::geometry::Runtime::Id>
    {
        const auto& generator = with<SkySphereGenerator>::get(context, asset_id);
        return with<rmmr::resource::geometry::Asset>::install(context, asset_id, device, build_cpu(generator));
    }

    auto ScrapBox::materialize(Writing context, rmmr::resource::geometry::Asset::Id assetId, rmmr::system::Device::Id device)
        -> optional<rmmr::resource::geometry::Runtime::Id>
    {
        using SurfaceId = rmmr::resource::geometry::SurfaceId;
        // kube() face order is +Z −Z +X −X +Y −Y; boxOf puts plate normal on Z, so the first four tris are hull, the rest are cuts.
        auto cpu = rmmr::resource::builders::geometry::GeometryGenerator::kube();
        vector<SurfaceId> primitiveSurfaces(12, SurfaceId{1});
        for (std::size_t triangle = 0; triangle < 4; ++triangle)
            primitiveSurfaces[triangle] = SurfaceId{0};
        return with<rmmr::resource::geometry::Asset>::install(context, assetId, device, cpu, primitiveSurfaces, umap<string, SurfaceId>{{"face", SurfaceId{0}}, {"cut", SurfaceId{1}}});
    }

}
