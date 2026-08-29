#include "scenarios/boundersAndStones.h"

#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::scenario {

    using namespace rmmr;

    namespace {

        constexpr float coreRadius = 20.0f;
        constexpr float coreSurfaceAccel = 2.0f;
        constexpr float coreKelvin = 6000.0f;
        constexpr float ringRadius = 90.0f;
        constexpr float clearanceGap = 8.0f;
        constexpr float pebbleRevPerSecMax = 2.0f;
        constexpr int mixChannels = 16;
        constexpr int ironMineral = 6;
        constexpr int pebbleCount = 100;
        constexpr int hornCount = 4;

        auto nibble(int channel, int fill) -> locality::geo::Mix {
            return locality::geo::Mix{static_cast<std::uint64_t>(fill)} << (channel * 4);
        }

        auto mixDensity(locality::geo::Mix mix) -> float {
            const auto& table = locality::geo::Mineral::table();
            float density = 0.0f;
            const auto channels = table.size() < static_cast<std::size_t>(mixChannels) ? table.size() : static_cast<std::size_t>(mixChannels);
            for (std::size_t channel = 0; channel < channels; ++channel) {
                const float fill = static_cast<float>((mix >> (channel * 4)) & 0xF) / 15.0f;
                density += fill * table[channel].kgPerCubicMeter();
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

    void BoundersAndStones::loadResources(Writing context, const rmmr::wrapper::assets::Handles& shared) {
        using namespace ::rmmr::resource;
        using AssetsHost = ::rmmr::resource::Assets;
        using Name = Unit::Name;
        using Material = ::rmmr::resource::material::Asset;

        if (not shared.material.lit)
            return (void)context.refuse("eltanin::scenario::BoundersAndStones::loadResources: rmmr lit missing");

        const auto rockShader = with<AssetsHost>::add_shader_loader(
            context,
            Name::from("Eltanin", "rock"),
            item<shader::Loader>{
                .vertex = "shaders/rock.vert.glsl",
                .fragment = "shaders/rock.frag.glsl",
            });
        const auto& litQuantum = with<Material>::get(context, *shared.material.lit);
        const auto shadowTechnique = litQuantum.techniques.find(renderer::Pass::shadow);
        if (shadowTechnique == litQuantum.techniques.end())
            return (void)context.refuse("eltanin::scenario::BoundersAndStones::loadResources: lit shadow technique missing");
        assets.rock = with<AssetsHost>::add_material(
            context,
            Name::from("Eltanin", "rock"),
            Material::Quantum{
                .techniques = {
                    {renderer::Pass::opaque, Material::Technique{
                        .program = with<Unit>::remember(context, rockShader),
                        .uniforms = ::rmmr::material::Semantics::ids_of({"shadowMap", "minerals"}),
                        .glowSpread = true,
                    }},
                    {renderer::Pass::shadow, Material::Technique{
                        .program = shadowTechnique->second.program,
                        .uniforms = {},
                        .glowSpread = false,
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
                        .glowSpread = true,
                    }},
                    {renderer::Pass::shadow, Material::Technique{
                        .program = shadowTechnique->second.program,
                        .uniforms = {},
                        .glowSpread = false,
                    }},
                },
                .nearest = false,
                .blend = renderer::BlendMode::inherit,
            });

        const auto manager = with<Manager>::singleton(context);
        const auto crustId = with<Unit_group>::addElement(context, manager, Unit::Quantum{.name = Name::from("Eltanin", "crust")});
        with<texture3array::Asset>::extend(context, crustId, texture3array::Asset::Quantum{.layerSize = index3{0, 0, 0}, .capacity = 0});
        assets.crust = crustId;
    }

    void BoundersAndStones::populate(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device) {
        const locality::geo::Mix palettes[hornCount]{
            nibble(1, 8) | nibble(2, 4) | nibble(3, 3),
            nibble(0, 15),
            nibble(6, 9) | nibble(7, 6),
            nibble(5, 11) | nibble(4, 4),
        };
        const float potatoDiameters[hornCount]{40.0f, 30.0f, 22.0f, 18.0f};
        std::mt19937 rng{20260817};
        std::normal_distribution<float> gauss{0.0f, 1.0f};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        const phys::rigid::CelestialGravity::Quantum coreGravity{.averageRadius = coreRadius, .surfaceAcceleration = coreSurfaceAccel};
        const float spinK = (twoPi / 180.0f) * sphereMass(coreRadius * 2.0f, locality::geo::Mineral::table()[static_cast<std::size_t>(ironMineral)].kgPerCubicMeter());

        auto orbitVelocity = [&](vec3 position) -> vec3 {
            const float distance = glm::length(position);
            if (distance <= 1.0e-6f)
                return vec3{0.0f, 0.0f, 0.0f};
            return vec3{-position.z, 0.0f, position.x} * (coreGravity.roundOrbitHelper(distance) / distance);
        };

        rocks.reserve(static_cast<std::size_t>(1 + hornCount));
        boulders.reserve(static_cast<std::size_t>(pebbleCount));
        {
            const locality::geo::GeneralizedRecipe recipe{
                .mix = locality::geo::GeneralizedRecipe::homogenous(ironMineral),
                .radius = coreRadius,
                .lump = 0.0f,
                .seed = 1,
                .spotMeters = 8.0f,
                .spotContrast = 0.0f,
            };
            const auto id = with<locality::geo::Rock>::spawnGenerated(context, root, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), recipe, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.08f, 0.0f});
            rocks.push_back(id);
            const auto& rock = with<locality::geo::Rock>::get(context, id);
            with<phys::rigid::CelestialGravity>::extend(context, rock.body, coreGravity);
            auto crystal = with<phys::rigid::Crystal>::modify(context, rock.body);
            for (phys::Particle& particle : crystal->particles) {
                particle.temperature = coreKelvin;
                particle.cohesion = 0.25f;
            }
            crystal->refreshMatter(*with<phys::Body>::modify(context, rock.body));
            with<rmmr::scene::actor::MeshState>::modify(context, rock.actor)->heat = vec2{coreKelvin, 0.25f};
        }

        Occupied occupied[hornCount];
        for (int index = 0; index < hornCount; ++index) {
            const float diameter = potatoDiameters[index];
            const float azim = twoPi * static_cast<float>(index) / static_cast<float>(hornCount);
            const Pose pose = ringPose(azim, rng);
            const locality::geo::GeneralizedRecipe recipe{
                .mix = palettes[index],
                .radius = diameter * 0.5f,
                .lump = glm::clamp(0.78f + 0.22f * unit(rng), 0.72f, 1.0f),
                .seed = 1100 + index,
                .spotMeters = glm::clamp(diameter * (0.22f + 0.24f * unit(rng)), 8.0f, diameter * 0.48f),
                .spotContrast = glm::clamp(0.78f + 0.14f * unit(rng), 0.70f, 1.0f),
            };
            occupied[index] = Occupied{.position = pose.position, .radius = diameter * 0.5f};
            rocks.push_back(with<locality::geo::Rock>::spawnGenerated(context, root, device, pose, recipe, orbitVelocity(pose.position), spinOmega(sphereMass(diameter, mixDensity(recipe.mix)), spinK, 1.0f, rng)));
        }

        for (int slot = 0; slot < pebbleCount; ++slot) {
            const float diameter = 0.5f + 1.5f * unit(rng);
            const float azim = twoPi * static_cast<float>(slot) / static_cast<float>(pebbleCount);
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
            const locality::geo::GeneralizedRecipe recipe{
                .mix = locality::geo::GeneralizedRecipe::homogenous(mineral),
                .radius = diameter * 0.5f,
                .lump = glm::clamp(0.40f + 0.25f * gauss(rng), 0.15f, 1.0f),
                .seed = 4100 + slot,
                .spotMeters = diameter,
                .spotContrast = 0.0f,
            };
            const float mass = sphereMass(diameter, locality::geo::Mineral::table()[static_cast<std::size_t>(mineral)].kgPerCubicMeter());
            boulders.push_back(with<locality::geo::Boulder>::spawnGenerated(context, root, device, pose, recipe, orbitVelocity(pose.position), spinOmega(mass, spinK, unit(rng), rng)));
        }
    }

}
