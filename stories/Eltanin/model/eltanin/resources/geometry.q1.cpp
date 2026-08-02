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

#include <glm/geometric.hpp>

namespace eltanin::resources {

    using namespace fqsm::api;
    using namespace rmmr;
    using rmmr::resource::builders::geometry::CpuPresentation;

    namespace {

        constexpr float k_mesh_radius = 100.0f;
        constexpr float k_atlas_size = 5.0f;
        constexpr float k_star_texels = 5.0f;
        constexpr float k_ly_per_pc = 3.261563777f;
        constexpr float k_m_sun_abs = 4.83f;
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
            glm::vec3 direction{};
            float half = 1.0f;
            vec4 color{1.0f};
            BillboardUv uv{};
        };

        auto quad_size_from_angular_diameter(float mesh_radius, float angular_diameter_deg) -> float {
            const float degrees = std::max(angular_diameter_deg, 0.05f);
            const float theta = degrees * std::numbers::pi_v<float> / 180.0f;
            return 2.0f * mesh_radius * std::tan(0.5f * theta);
        }

        auto temperature_rgb(float temperature_K) -> vec3 {
            const float t = std::clamp(temperature_K, 1000.0f, 40000.0f) / 100.0f;
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            if (t <= 66.0f) {
                r = 1.0f;
                g = std::clamp(0.39008157876f * std::log(t) - 0.63184144378f, 0.0f, 1.0f);
            } else {
                r = std::clamp(1.29293618606f * std::pow(t - 60.0f, -0.1332047592f), 0.0f, 1.0f);
                g = std::clamp(1.12989086089f * std::pow(t - 60.0f, -0.0755148492f), 0.0f, 1.0f);
            }
            if (t >= 66.0f) {
                b = 1.0f;
            } else if (t <= 19.0f) {
                b = 0.0f;
            } else {
                b = std::clamp(0.54320678911f * std::log(t - 10.0f) - 1.19625408914f, 0.0f, 1.0f);
            }
            return vec3{r, g, b};
        }

        auto apparent_magnitude_from_luminosity(float luminosity_sun, float distance_ly) -> float {
            const float distance_pc = std::max(distance_ly / k_ly_per_pc, 1.0e-3f);
            const float absolute = k_m_sun_abs - 2.5f * std::log10(std::max(luminosity_sun, 1.0e-8f));
            return absolute + 5.0f * std::log10(distance_pc) - 5.0f;
        }

        auto direction_and_distance(const glm::vec3& position_ly) -> std::pair<glm::vec3, float> {
            const glm::vec3 offset = position_ly - k_observer_ly;
            const float distance = glm::length(offset);
            if (distance < 1.0f) {
                return {glm::vec3{0.0f, 1.0f, 0.0f}, 1.0f};
            }
            return {offset / distance, distance};
        }

        void emit_billboard(CpuPresentation& cpu, const PendingBillboard& billboard) {
            const vec3 center = vec3{billboard.direction} * k_mesh_radius;
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

            const BillboardUv star_uv{0.0f, 0.0f, k_star_texels / k_atlas_size, k_star_texels / k_atlas_size};
            const float star_half = 0.5f * quad_size_from_angular_diameter(k_mesh_radius, quantum.angular_diameter_deg);

            CpuPresentation cpu{
                .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "uv0", "color0"}),
                .positions = {},
                .normals = {},
                .uv0 = {},
                .color0 = {},
                .indices = {},
            };
            cpu.positions.reserve(galaxy.size() * 6);
            cpu.uv0.reserve(galaxy.size() * 6);
            cpu.color0.reserve(galaxy.size() * 6);

            for (const Star& star : galaxy) {
                const auto [direction, distance] = direction_and_distance(star.position_ly);
                const float magnitude = apparent_magnitude_from_luminosity(star.luminosity_sun, distance);
                // Near-field pop: ~50–100 local stars should read as bright neighbors (not just one).
                // Soft 1/√d relative to ~250 ly, saturating so the very closest don't blow out alone.
                constexpr float k_near_ly = 250.0f;
                const float near_boost = std::clamp(
                    std::sqrt(k_near_ly / std::max(distance, 25.0f)),
                    0.55f,
                    3.2f);
                const float size_scale = std::clamp(
                    std::pow(10.0f, -0.20f * (magnitude - 1.5f)) * near_boost,
                    0.28f,
                    7.0f);
                const float brightness = std::clamp(
                    std::pow(10.0f, -0.28f * (magnitude - 5.0f)) * near_boost,
                    0.30f,
                    7.0f);
                const vec3 rgb = temperature_rgb(star.temperature_K) * brightness;
                emit_billboard(cpu, PendingBillboard{
                    .direction = direction,
                    .half = star_half * size_scale,
                    .color = vec4{rgb, 1.0f},
                    .uv = star_uv,
                });
            }

            base::message("eltanin::SkySphereGenerator: stars={}", galaxy.size());
            return cpu;
        }

    } // namespace

    auto SkySphereGenerator::Actions::materialize(Writing context, Id asset_id, rmmr::system::Device::Id device)
        -> optional<rmmr::resource::geometry::Runtime::Id>
    {
        const auto& generator = with<SkySphereGenerator>::get(context, asset_id);
        return with<rmmr::resource::geometry::Asset>::install(context, asset_id, device, build_cpu(generator));
    }

    auto AtomicVisualizer::Actions::materialize(Writing context, Id asset_id, rmmr::system::Device::Id device)
        -> optional<rmmr::resource::geometry::Runtime::Id>
    {
        using AtomicAsset = ::eltanin::resource::atomic::Asset;
        const auto& generator = with<AtomicVisualizer>::get(context, asset_id);
        if (not with<AtomicAsset>::exists(context, generator.source)) {
            return context.refuse("eltanin::AtomicVisualizer: source atomic::Asset missing");
        }
        const auto& atomic = with<AtomicAsset>::get(context, generator.source);
        if (atomic.points.empty()) {
            return context.refuse("eltanin::AtomicVisualizer: atomic points are empty");
        }
        if (atomic.faces.empty()) {
            return context.refuse("eltanin::AtomicVisualizer: atomic faces are empty");
        }

        CpuPresentation cpu{
            .layout = rmmr::primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"}),
            .positions = {},
            .normals = {},
            .uv0 = {},
            .color0 = {},
            .indices = {},
        };
        umap<string, rmmr::resource::geometry::Asset::Part> parts;

        for (const auto& face : atomic.faces) {
            if (face.name.empty()) return context.refuse("eltanin::AtomicVisualizer: face name is empty");
            if (parts.contains(face.name)) return context.refuse(std::format("eltanin::AtomicVisualizer: duplicate face name '{}'", face.name));
            if (face.indices.size() < 3) return context.refuse(std::format("eltanin::AtomicVisualizer: face '{}' needs at least 3 indices", face.name));

            for (const auto index : face.indices) {
                if (index < 0 || static_cast<std::size_t>(index) >= atomic.points.size()) {
                    return context.refuse(std::format("eltanin::AtomicVisualizer: face '{}' index {} out of range", face.name, index));
                }
            }

            const glm::vec3 p0{atomic.points[static_cast<std::size_t>(face.indices[0])]};
            const glm::vec3 p1{atomic.points[static_cast<std::size_t>(face.indices[1])]};
            const glm::vec3 p2{atomic.points[static_cast<std::size_t>(face.indices[2])]};
            const glm::vec3 crossed = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(crossed) <= 0.0f) {
                return context.refuse(std::format("eltanin::AtomicVisualizer: face '{}' has degenerate normal", face.name));
            }
            const glm::vec3 n = glm::normalize(crossed);
            const glm::vec3 edge = p1 - p0;
            if (glm::length(edge) <= 0.0f) {
                return context.refuse(std::format("eltanin::AtomicVisualizer: face '{}' has degenerate U axis", face.name));
            }
            const glm::vec3 u_axis = glm::normalize(edge);
            const glm::vec3 v_axis = glm::normalize(glm::cross(n, u_axis));
            const Pos face_normal{n};

            const auto vertex_base = integer(cpu.positions.size());
            const auto start_index = rmmr::renderer::Count(cpu.indices.size());
            for (const auto index : face.indices) {
                const glm::vec3 position{atomic.points[static_cast<std::size_t>(index)]};
                const glm::vec3 relative = position - p0;
                cpu.positions.push_back(Pos{position});
                cpu.normals.push_back(face_normal);
                cpu.uv0.push_back(UV{glm::dot(relative, u_axis), glm::dot(relative, v_axis)});
            }
            for (std::size_t corner = 1; corner + 1 < face.indices.size(); ++corner) {
                cpu.indices.push_back(vertex_base);
                cpu.indices.push_back(vertex_base + integer(corner));
                cpu.indices.push_back(vertex_base + integer(corner + 1));
            }
            parts.emplace(face.name, rmmr::resource::geometry::Asset::Part{
                .startIndex = start_index,
                .countIndex = rmmr::renderer::Count(cpu.indices.size()) - start_index,
            });
        }

        with<rmmr::resource::geometry::Asset>::modify(context, asset_id)->parts = std::move(parts);
        return with<rmmr::resource::geometry::Asset>::install(context, asset_id, device, cpu);
    }

}
