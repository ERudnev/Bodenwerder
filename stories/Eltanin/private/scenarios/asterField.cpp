#include "scenarios/asterField.h"

#include "mech/semantics/space.h"

#include <eltanin/geo/boulder.q1.h>
#include <eltanin/geo/rock.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void AsterField::loadResources(Writing context, const rmmr::wrapper::assets::Handles& shared) {
        using namespace ::rmmr::resource;
        using AssetsHost = ::rmmr::resource::Assets;
        using Name = Unit::Name;
        using Material = ::rmmr::resource::material::Asset;

        if (not shared.material.lit)
            return (void)context.refuse("eltanin::scenario::AsterField::loadResources: rmmr lit missing");

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
            return (void)context.refuse("eltanin::scenario::AsterField::loadResources: lit shadow technique missing");
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

    void AsterField::populate(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device) {
        constexpr int pgmMineral = 11;
        constexpr int countY = 30;
        constexpr int countZ = 52;
        constexpr float pebbleRadius = 0.15f;
        constexpr float pad = 0.4f;
        constexpr vec3 drift{4.0f, 0.0f, 0.0f};
        const float cell = mech::space::local::edge2meters;
        const float spanY = 2.0f * (cell + 2.0f * pad);
        const float spanZ = 2.0f * (cell * 2.0f + 2.0f * pad);
        const vec3 cloudCenter{-8.0f, 0.5f * cell, cell};
        const vec3 stepY{0.0f, spanY / static_cast<float>(countY - 1), 0.0f};
        const vec3 stepZ{0.0f, 0.0f, spanZ / static_cast<float>(countZ - 1)};
        const float originY = 0.5f * static_cast<float>(countY - 1);
        const float originZ = 0.5f * static_cast<float>(countZ - 1);
        for (int row = 0; row < countY; ++row) {
            for (int col = 0; col < countZ; ++col) {
                const vec3 position = cloudCenter + stepY * (static_cast<float>(row) - originY) + stepZ * (static_cast<float>(col) - originZ);
                const geo::GeneralizedRecipe recipe{
                    .mix = geo::GeneralizedRecipe::homogenous(pgmMineral),
                    .radius = pebbleRadius,
                    .lump = 0.4f,
                    .seed = 11 + row * countZ + col,
                    .spotMeters = pebbleRadius * 2.0f,
                    .spotContrast = 0.0f,
                };
                with<geo::Boulder>::spawnGenerated(context, root, device, Pose::from(Pos{position}, HPB{0.0f, 0.0f, 0.0f}), recipe, drift, vec3{0.0f, 0.0f, 0.0f});
            }
        }
    }

}
