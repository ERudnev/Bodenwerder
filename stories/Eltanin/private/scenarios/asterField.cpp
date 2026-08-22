#include "scenarios/asterField.h"

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

#include <cmath>
#include <numbers>
#include <random>

namespace eltanin::scenario {

    using namespace rmmr;

    namespace {

        constexpr float diskRadius = 400.0f;
        constexpr float diskThickness = 10.0f;
        constexpr int pebbleCount = 1000;
        constexpr integer iron = 6;
        constexpr integer ice = 0;
        constexpr integer silicates[]{1, 2, 3};

        auto pickMix(std::mt19937& rng) -> geo::Mix {
            std::uniform_real_distribution<float> unit{0.0f, 1.0f};
            const float roll = unit(rng);
            if (roll < 0.10f)
                return geo::Rock::GeneralizedRecipe::homogenous(iron);
            if (roll < 0.35f)
                return geo::Rock::GeneralizedRecipe::homogenous(ice);
            std::uniform_int_distribution<int> silicate{0, 2};
            return geo::Rock::GeneralizedRecipe::homogenous(silicates[silicate(rng)]);
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
        {
            auto place = with<scene::Root>::modify(context, root);
            place->fog = {.color = RGB{0.55f, 0.52f, 0.48f}, .density = 0.0012f, .height = 0.0f, .heightFalloff = 0.12f, .maxOpacity = 0.72f, .distanceScale = 1.0f};
            place->gravity = vec3{0.0f, 0.0f, 0.0f};
        }
        std::mt19937 rng{20260822};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        for (int slot = 0; slot < pebbleCount; ++slot) {
            const float azim = twoPi * unit(rng);
            const float radius = diskRadius * std::sqrt(unit(rng));
            const float y = (unit(rng) - 0.5f) * diskThickness;
            const float diameter = 0.35f + 1.15f * unit(rng);
            const geo::Rock::GeneralizedRecipe recipe{
                .mix = pickMix(rng),
                .radius = diameter * 0.5f,
                .lump = 0.35f + 0.40f * unit(rng),
                .seed = 8000 + slot,
                .spotMeters = diameter,
                .spotContrast = 0.0f,
            };
            with<geo::Rock>::spawnGenerated(context, root, device, Pose::from(Pos{radius * std::cos(azim), y, radius * std::sin(azim)}, HPB{360.0f * unit(rng), 0.0f, 0.0f}), recipe, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        }
    }

}
