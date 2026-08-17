#include "scenarios/boundersAndStones.h"

#include <eltanin/geo/minerals.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::scenarios {

    using namespace rmmr;

    namespace {

        constexpr float ringRadius = 800.0f;
        constexpr float boulderStep = 20.0f;
        constexpr float clearanceGap = 8.0f;
        constexpr float giantPeriodSeconds = 180.0f;
        constexpr float pebbleRevPerSecMax = 2.0f;
        constexpr int mixChannels = 16;

        auto nibble(int channel, int fill) -> geo::Mix {
            return geo::Mix{static_cast<std::uint64_t>(fill)} << (channel * 4);
        }

        auto mixDensity(geo::Mix mix) -> float {
            const auto& table = geo::Mineral::table();
            float density = 0.0f;
            const auto channels = table.size() < static_cast<std::size_t>(mixChannels) ? table.size() : static_cast<std::size_t>(mixChannels);
            for (std::size_t channel = 0; channel < channels; ++channel) {
                const float fill = static_cast<float>((mix >> (channel * 4)) & 0xF) / 15.0f;
                density += fill * table[channel].density;
            }
            return density;
        }

        auto sphereMass(float diameter, float density) -> float {
            const float radius = diameter * 0.5f;
            return density * (4.0f / 3.0f) * std::numbers::pi_v<float> * radius * radius * radius;
        }

        auto spinOmega(float mass, float spinK, float jitter, std::mt19937& rng) -> vec3 {
            std::normal_distribution<float> gauss{0.0f, 1.0f};
            float magnitude = spinK / glm::max(mass, 1.0e-6f);
            magnitude = glm::min(magnitude, pebbleRevPerSecMax * 2.0f * std::numbers::pi_v<float>);
            magnitude *= jitter;
            vec3 axis{gauss(rng), gauss(rng), gauss(rng)};
            if (glm::dot(axis, axis) < 1.0e-8f)
                axis = vec3{0.0f, 1.0f, 0.0f};
            return glm::normalize(axis) * magnitude;
        }

        auto ringPose(float azim, std::mt19937& rng) -> Pose {
            std::uniform_real_distribution<float> unit{0.0f, 1.0f};
            std::normal_distribution<float> gauss{0.0f, 1.0f};
            return Pose::from(Pos{ringRadius * std::cos(azim), 0.0f, ringRadius * std::sin(azim)}, HPB{360.0f * unit(rng), 30.0f * gauss(rng), 360.0f * unit(rng)});
        }

        struct Occupied {
            vec3 position;
            float radius;
        };

    } // namespace

    auto BoundersAndStones::loadResources(Writing context, const rmmr::wrapper::assets::Handles& shared) -> bool {
        using namespace ::rmmr::resource;
        using AssetsHost = ::rmmr::resource::Assets;
        using Name = Unit::Name;
        using Material = ::rmmr::resource::material::Asset;

        if (not shared.material.lit) {
            context.refuse("eltanin::scenarios::BoundersAndStones::loadResources: rmmr lit missing");
            return false;
        }

        const auto rockShader = with<AssetsHost>::add_shader_loader(
            context,
            Name::from("Eltanin", "rock"),
            item<shader::Loader>{
                .vertex = "shaders/rock.vert.glsl",
                .fragment = "shaders/rock.frag.glsl",
            });
        const auto& litQuantum = with<Material>::get(context, *shared.material.lit);
        const auto shadowTechnique = litQuantum.techniques.find(renderer::Pass::shadow);
        if (shadowTechnique == litQuantum.techniques.end()) {
            context.refuse("eltanin::scenarios::BoundersAndStones::loadResources: lit shadow technique missing");
            return false;
        }
        assets.rock = with<AssetsHost>::add_material(
            context,
            Name::from("Eltanin", "rock"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::opaque, Material::Technique{
                        .program = with<Unit>::remember(context, rockShader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}),
                    }},
                    {renderer::Pass::shadow, Material::Technique{
                        .program = shadowTechnique->second.program,
                        .uniforms = {},
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::inherit,
            });

        const auto boulderShader = with<AssetsHost>::add_shader_loader(
            context,
            Name::from("Eltanin", "boulder"),
            item<shader::Loader>{
                .vertex = "shaders/boulder.vert.glsl",
                .fragment = "shaders/boulder.frag.glsl",
            });
        assets.boulder = with<AssetsHost>::add_material(
            context,
            Name::from("Eltanin", "boulder"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::opaque, Material::Technique{
                        .program = with<Unit>::remember(context, boulderShader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}),
                    }},
                    {renderer::Pass::shadow, Material::Technique{
                        .program = shadowTechnique->second.program,
                        .uniforms = {},
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::inherit,
            });

        const auto manager = with<Manager>::singleton(context);
        if (not manager) {
            context.refuse("eltanin::scenarios::BoundersAndStones::loadResources: resource Manager missing");
            return false;
        }
        if (not with<Unit_group>::exists(context, *manager))
            with<Unit_group>::extend(context, *manager);
        const auto crustId = with<Unit_group>::addElement(context, *manager, Unit::Quantum{.name = Name::from("Eltanin", "crust")});
        with<texture3array::Asset>::extend(context, crustId, texture3array::Asset::Quantum{.layerSize = index3{0, 0, 0}, .capacity = 0});
        assets.crust = crustId;
        return true;
    }

    void BoundersAndStones::populate(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device) {
        const geo::Mix palettes[5]{
            nibble(1, 8) | nibble(2, 4) | nibble(3, 3),
            nibble(0, 15),
            nibble(6, 9) | nibble(7, 6),
            nibble(5, 11) | nibble(4, 4),
            nibble(9, 7) | nibble(8, 5) | nibble(3, 3),
        };
        const float potatoDiameters[5]{400.0f, 200.0f, 100.0f, 50.0f, 25.0f};
        std::mt19937 rng{20260817};
        std::normal_distribution<float> gauss{0.0f, 1.0f};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        const vec3 rest{0.0f, 0.0f, 0.0f};
        const float giantMass = sphereMass(potatoDiameters[0], mixDensity(palettes[0]));
        const float spinK = (twoPi / giantPeriodSeconds) * giantMass;

        Occupied occupied[5];
        rocks.reserve(5);
        for (int index = 0; index < 5; ++index) {
            const float diameter = potatoDiameters[index];
            const float azim = twoPi * static_cast<float>(index) / 5.0f;
            const Pose pose = ringPose(azim, rng);
            const geo::Recipe recipe{
                .mix = palettes[index],
                .spotMeters = glm::clamp(diameter * 0.12f, 4.0f, 48.0f),
                .spotContrast = glm::clamp(0.40f + 0.20f * gauss(rng), 0.10f, 0.90f),
                .diameterMeters = diameter,
                .lump = glm::clamp(0.40f + 0.20f * gauss(rng), 0.18f, 0.85f),
                .seed = 1100 + index,
            };
            occupied[index] = Occupied{.position = pose.position, .radius = diameter * 0.5f};
            rocks.push_back(with<geo::Rock>::spawnGenerated(context, root, device, pose, recipe, rest, spinOmega(sphereMass(diameter, mixDensity(recipe.mix)), spinK, 1.0f, rng)));
        }

        const int boulderSlots = static_cast<int>(twoPi * ringRadius / boulderStep);
        boulders.reserve(static_cast<std::size_t>(boulderSlots));
        for (int slot = 0; slot < boulderSlots; ++slot) {
            const float diameter = 0.5f + 9.5f * unit(rng);
            const float azim = static_cast<float>(slot) * boulderStep / ringRadius;
            const Pose pose = ringPose(azim, rng);
            bool blocked = false;
            for (const auto& body : occupied) {
                if (glm::length(pose.position - body.position) < body.radius + diameter * 0.5f + clearanceGap) {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
                continue;
            const integer mineral = static_cast<integer>(slot % 16);
            const geo::Boulder::Recipe recipe{
                .mineral = mineral,
                .diameterMeters = diameter,
                .lump = glm::clamp(0.40f + 0.25f * gauss(rng), 0.15f, 1.0f),
                .seed = 4100 + slot,
            };
            const float mass = sphereMass(diameter, geo::Mineral::table()[static_cast<std::size_t>(mineral)].density);
            boulders.push_back(with<geo::Boulder>::spawn(context, root, device, pose, recipe, rest, spinOmega(mass, spinK, unit(rng), rng)));
        }
    }

}
