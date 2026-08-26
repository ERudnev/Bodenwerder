#include "scenarios/asterField.h"

#include <eltanin/geo/boulder.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

#include <cstdint>

namespace eltanin::scenario {

    using namespace rmmr;

    namespace {

        constexpr float coreRadius = 8.1f;
        constexpr float pebbleRadius = 2.4f;
        constexpr integer shrapnelCount = 20;
        constexpr float shrapnelSpeed = 100.0f;
        constexpr vec3 shrapnelOrigin{-200.0f, 2.0f, 0.0f};
        constexpr vec3 shrapnelStride{5.0f, 0.0f, 0.0f};
        constexpr integer feldspar = 3;
        constexpr integer olivine = 1;

        auto nibble(integer channel, integer fill) -> geo::Mix {
            return geo::Mix{static_cast<std::uint64_t>(fill)} << (channel * 4);
        }

        auto stoneCoreMix() -> geo::Mix {
            return nibble(olivine, 8) | nibble(feldspar, 7);
        }

    } // namespace

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
        with<scene::Root>::modify(context, root)->atmosphereDensity = 1225.0f * 0.01f;
        const geo::GeneralizedRecipe core{
            .mix = stoneCoreMix(),
            .radius = coreRadius,
            .lump = 0.0f,
            .seed = 7001,
            .spotMeters = coreRadius * 0.45f,
            .spotContrast = 0.12f,
        };
        with<geo::Rock>::spawnGenerated(context, root, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), core, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        // parked: first hooligan — keep the lump, useful shape
        // constexpr float banditRadius = coreRadius * 2.0f;
        // constexpr float banditX = -80.0f;
        // constexpr float banditSpeed = 160.0f;
        // constexpr integer ice = 0;
        // const geo::GeneralizedRecipe bandit{
        //     .mix = geo::GeneralizedRecipe::homogenous(ice),
        //     .radius = banditRadius,
        //     .lump = 0.82f,
        //     .seed = 20260818,
        //     .spotMeters = banditRadius * 0.48f,
        //     .spotContrast = 0.0f,
        // };
        // with<geo::Rock>::spawnGenerated(context, root, device, Pose::from(Pos{banditX, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), bandit, vec3{banditSpeed, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        for (integer n = 0; n < shrapnelCount; ++n) {
            const geo::GeneralizedRecipe shard{
                .mix = stoneCoreMix(),
                .radius = pebbleRadius,
                .lump = 0.3f,
                .seed = 4101 + n,
                .spotMeters = pebbleRadius * 0.5f,
                .spotContrast = 0.1f,
            };
            const vec3 pos = shrapnelOrigin + float(n) * shrapnelStride;
            with<geo::Boulder>::spawnGenerated(context, root, device, Pose::from(Pos{pos.x, pos.y, pos.z}, HPB{0.0f, 0.0f, 0.0f}), shard, vec3{shrapnelSpeed, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        }
    }

}
