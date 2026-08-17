#include "scenarios/boundersAndStones.h"

#include "physics/system.h"

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

        auto circularVelocity(vec3 position) -> vec3 {
            const float radius = glm::length(position);
            if (radius < 1.0f)
                return vec3{0.0f, 0.0f, 0.0f};
            vec3 tangent = glm::cross(vec3{0.0f, 1.0f, 0.0f}, position);
            if (glm::dot(tangent, tangent) < 1.0e-8f)
                tangent = glm::cross(vec3{1.0f, 0.0f, 0.0f}, position);
            return glm::normalize(tangent) * std::sqrt(phys::Settings::centralMu / radius);
        }

        auto nibble(int channel, int fill) -> geo::Mix {
            return geo::Mix{static_cast<std::uint64_t>(fill)} << (channel * 4);
        }

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
        // with<geo::Rock>::spawnIceSphere(context, root, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
        // with<geo::Rock>::spawnPaletteTorus(context, root, device, Pose::from(Pos{80.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));

        const geo::Mix palettes[10]{
            nibble(0, 15),
            nibble(1, 8) | nibble(3, 7),
            nibble(1, 10) | nibble(2, 4) | nibble(3, 1),
            nibble(1, 6) | nibble(2, 3) | nibble(6, 4) | nibble(7, 2),
            nibble(5, 11) | nibble(4, 3) | nibble(1, 1),
            nibble(6, 9) | nibble(7, 5) | nibble(8, 1),
            nibble(0, 8) | nibble(1, 4) | nibble(14, 3),
            nibble(3, 7) | nibble(4, 4) | nibble(9, 4),
            nibble(2, 10) | nibble(11, 3) | nibble(6, 2),
            nibble(5, 8) | nibble(0, 5) | nibble(15, 2),
        };
        std::mt19937 rng{20260817};
        std::normal_distribution<float> gauss{0.0f, 1.0f};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        constexpr float goldenAzim = 137.508f;
        constexpr float periodMin = 2.5f;
        constexpr float periodMax = 60.0f;
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        rocks.reserve(50);
        for (int index = 0; index < 50; ++index) {
            const float diameter = (index < 2) ? 100.0f : 12.0f + 13.0f * unit(rng);
            const float period = periodMin * std::pow(periodMax / periodMin, unit(rng));
            const float orbitKepler = std::cbrt(phys::Settings::centralMu * period * period / (twoPi * twoPi));
            const float orbit = glm::max(orbitKepler, diameter * 0.55f + 40.0f);
            const float azim = (goldenAzim * static_cast<float>(index) + 8.0f * gauss(rng)) * std::numbers::pi_v<float> / 180.0f;
            const Pose pose = Pose::from(Pos{orbit * std::cos(azim), 0.0f, orbit * std::sin(azim)}, HPB{360.0f * unit(rng), 30.0f * gauss(rng), 360.0f * unit(rng)});
            const geo::Recipe recipe{
                .mix = palettes[index % 10],
                .spotMeters = glm::clamp(diameter * 0.22f, 4.0f, 28.0f),
                .spotContrast = glm::clamp(0.35f + 0.25f * gauss(rng), 0.05f, 0.90f),
                .diameterMeters = diameter,
                .lump = glm::clamp(0.35f + 0.90f * gauss(rng), 0.12f, 0.99f),
                .seed = 1100 + index,
            };
            const vec3 omega{0.35f * gauss(rng), 0.55f * gauss(rng), 0.35f * gauss(rng)};
            rocks.push_back(with<geo::Rock>::spawnGenerated(context, root, device, pose, recipe, circularVelocity(pose.position), omega));
        }

        std::mt19937 debrisRng{20260818};
        boulders.reserve(220);
        for (int index = 0; index < 220; ++index) {
            const float diameter = 0.5f + 3.5f * unit(debrisRng);
            const float period = periodMin * std::pow(periodMax / periodMin, unit(debrisRng));
            const float orbitKepler = std::cbrt(phys::Settings::centralMu * period * period / (twoPi * twoPi));
            const float orbit = glm::max(orbitKepler, diameter * 0.55f + 40.0f);
            const float azim = (goldenAzim * static_cast<float>(index) + 41.0f + 6.0f * gauss(debrisRng)) * std::numbers::pi_v<float> / 180.0f;
            const Pose pose = Pose::from(Pos{orbit * std::cos(azim), 0.0f, orbit * std::sin(azim)}, HPB{360.0f * unit(debrisRng), 30.0f * gauss(debrisRng), 360.0f * unit(debrisRng)});
            const geo::Boulder::Recipe recipe{
                .mineral = static_cast<integer>(index % 16),
                .diameterMeters = diameter,
                .lump = glm::clamp(0.40f + 0.25f * gauss(debrisRng), 0.15f, 1.0f),
                .seed = 4100 + index,
            };
            const vec3 omega{0.55f * gauss(debrisRng), 0.75f * gauss(debrisRng), 0.55f * gauss(debrisRng)};
            boulders.push_back(with<geo::Boulder>::spawn(context, root, device, pose, recipe, circularVelocity(pose.position), omega));
        }
    }

}
