#include "scenarios/lavaAndRock.h"

#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void LavaAndRock::loadResources(Writing context, const rmmr::wrapper::assets::Handles& shared) {
        using namespace ::rmmr::resource;
        using AssetsHost = ::rmmr::resource::Assets;
        using Name = Unit::Name;
        using Material = ::rmmr::resource::material::Asset;

        if (not shared.material.lit)
            return (void)context.refuse("eltanin::scenario::LavaAndRock::loadResources: rmmr lit missing");

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
            return (void)context.refuse("eltanin::scenario::LavaAndRock::loadResources: lit shadow technique missing");
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

        const auto manager = *with<Manager>::singleton(context);
        const auto crustId = with<Unit_group>::addElement(context, manager, Unit::Quantum{.name = Name::from("Eltanin", "crust")});
        with<texture3array::Asset>::extend(context, crustId, texture3array::Asset::Quantum{.layerSize = index3{0, 0, 0}, .capacity = 0});
        assets.crust = crustId;
    }

    void LavaAndRock::populate(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device) {
        const auto brick = with<geo::Rock>::spawnLavaBrick(context, root, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
        rocks.push_back(brick);
        constexpr float brickKelvin = 1800.0f;
        {
            const auto& rock = with<geo::Rock>::get(context, brick);
            auto crystal = with<phys::rigid::Crystal>::modify(context, rock.body);
            for (phys::Particle& particle : crystal->particles)
                particle.temperature = brickKelvin;
            crystal->refreshMatter();
            with<rmmr::scene::actor::MeshState>::modify(context, rock.actor)->heat.x = brickKelvin;
        }

        constexpr int count = 50;
        constexpr int rows = 16;
        constexpr float diameter = 0.5f;
        constexpr float spacing = 1.0f;
        constexpr float rowStep = 2.0f;
        constexpr float kelvinMax = 2000.0f;
        rocks.reserve(1 + static_cast<std::size_t>(count) * rows);
        for (int row = 0; row < rows; ++row) {
            for (int index = 0; index < count; ++index) {
                const float kelvin = kelvinMax * static_cast<float>(index) / static_cast<float>(count - 1);
                const float x = 55.0f + spacing * static_cast<float>(index);
                const float z = 70.0f + rowStep * static_cast<float>(row);
                const geo::Rock::GeneralizedRecipe recipe{
                    .mix = geo::Rock::GeneralizedRecipe::homogenous(row),
                    .radius = diameter * 0.5f,
                    .lump = 0.55f,
                    .seed = 7 + index + row * 1000,
                    .spotMeters = diameter,
                    .spotContrast = 0.0f,
                };
                const auto id = with<geo::Rock>::spawnGenerated(context, root, device, Pose::from(Pos{x, 0.0f, z}, HPB{0.0f, 0.0f, 0.0f}), recipe, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
                rocks.push_back(id);
                const auto& rock = with<geo::Rock>::get(context, id);
                auto crystal = with<phys::rigid::Crystal>::modify(context, rock.body);
                for (phys::Particle& particle : crystal->particles)
                    particle.temperature = kelvin;
                crystal->refreshMatter();
                with<rmmr::scene::actor::MeshState>::modify(context, rock.actor)->heat.x = kelvin;
            }
        }
    }

}
