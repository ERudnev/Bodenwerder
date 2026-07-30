#include "galaxy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

#include <glm/geometric.hpp>

namespace kubes::resources {

    namespace {

        // Near-MW cartoon (1 kpc ≈ 3262 ly). Disk + extended stellar halo.
        constexpr float k_kpc_ly = 3261.56f;
        // Surface density Σ(R): flat core, then Gaussian wing — no exp spike at R=0.
        constexpr float k_disk_plateau_ly = 2.0f * k_kpc_ly;     // ~2 kpc plateau
        constexpr float k_disk_sigma_ly = 4.0f * k_kpc_ly;       // falloff width
        constexpr float k_disk_radius_max_ly = 15.0f * k_kpc_ly; // stellar disk ~15 kpc
        constexpr float k_disk_height_ly = 0.3f * k_kpc_ly;      // thin disk ~300 pc
        constexpr float k_halo_scale_ly = 30.0f * k_kpc_ly;      // big round stellar halo
        constexpr float k_arm_count = 4.0f;
        constexpr float k_arm_tightness = 0.18f;
        constexpr float k_arm_spread = 0.45f;
        constexpr float k_disk_fraction = 0.639f; // was 0.78; after 2× pop, halo doubled again → ~36% halo

        auto clamp01(float value) -> float {
            if (value < 0.0f) return 0.0f;
            if (value > 1.0f) return 1.0f;
            return value;
        }

        auto disk_surface_density(float radius_ly) -> float {
            if (radius_ly <= k_disk_plateau_ly) {
                return 1.0f;
            }
            const float x = (radius_ly - k_disk_plateau_ly) / k_disk_sigma_ly;
            return std::exp(-0.5f * x * x);
        }

        auto paint_star(float mass_sun, std::mt19937& rng) -> Star {
            std::uniform_real_distribution<float> jitter(0.85f, 1.15f);
            const float mass = std::max(0.08f, mass_sun);
            const float luminosity = std::pow(mass, 3.5f) * jitter(rng);
            const float temperature = 5800.0f * std::pow(mass, 0.55f) * jitter(rng);
            return Star{
                .position_ly = {},
                .temperature_K = std::clamp(temperature, 2500.0f, 40000.0f),
                .luminosity_sun = std::max(luminosity, 1.0e-4f),
                .mass_sun = mass,
            };
        }

        auto sample_mass(std::mt19937& rng) -> float {
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            const float u = std::max(unit(rng), 1.0e-4f);
            return std::clamp(std::pow(u, -0.7f) * 0.2f, 0.08f, 25.0f);
        }

        auto sample_disk_radius(std::mt19937& rng) -> float {
            // p(R) ∝ Σ(R)·R. Rejection vs bound R_max (loose but fine at 20k).
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            const float weight_bound = k_disk_radius_max_ly;
            for (;;) {
                const float radius = unit(rng) * k_disk_radius_max_ly;
                const float weight = disk_surface_density(radius) * radius;
                if (unit(rng) * weight_bound <= weight) {
                    return radius;
                }
            }
        }

        auto sample_disk(std::mt19937& rng) -> glm::vec3 {
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            std::normal_distribution<float> gauss(0.0f, 1.0f);

            const float radius = sample_disk_radius(rng);

            const float arm = std::floor(unit(rng) * k_arm_count);
            const float arm_phase = (arm / k_arm_count) * 2.0f * std::numbers::pi_v<float>;
            const float spiral = arm_phase + k_arm_tightness * std::log(std::max(radius, 500.0f) / 500.0f);
            const float theta = spiral + gauss(rng) * k_arm_spread;

            const float flare = 0.35f + 0.65f * clamp01(radius / k_disk_radius_max_ly);
            const float height = gauss(rng) * k_disk_height_ly * flare;
            return glm::vec3{
                radius * std::cos(theta),
                height,
                radius * std::sin(theta),
            };
        }

        auto sample_halo(std::mt19937& rng) -> glm::vec3 {
            std::normal_distribution<float> gauss(0.0f, 1.0f);
            glm::vec3 direction{gauss(rng), gauss(rng), gauss(rng)};
            const float length = glm::length(direction);
            if (length < 1.0e-5f) {
                direction = glm::vec3{1.0f, 0.0f, 0.0f};
            } else {
                direction /= length;
            }
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            const float radius = k_halo_scale_ly * std::pow(unit(rng), 0.55f);
            return direction * radius;
        }

    } // namespace

    auto generate_spiral_galaxy(std::size_t count, std::uint32_t seed) -> Galaxy {
        Galaxy galaxy;
        galaxy.reserve(count);
        std::mt19937 rng{seed};
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        for (std::size_t i = 0; i < count; ++i) {
            Star star = paint_star(sample_mass(rng), rng);
            star.position_ly = unit(rng) < k_disk_fraction ? sample_disk(rng) : sample_halo(rng);
            galaxy.push_back(star);
        }
        return galaxy;
    }

}
