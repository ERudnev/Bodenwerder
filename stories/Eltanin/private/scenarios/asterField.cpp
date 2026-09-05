#include "scenarios/asterField.h"

#include <eltanin/locality/geo/rock.q1.h>
#include <eltanin/locality/flash.q1.h>
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

    void AsterField::populate(Writing context, rmmr::system::Device::Id device) {
        // Temporary: flash spawn parked.
        //with<locality::Flash>::spawn(context, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f}, locality::Flash::Channels{.kinetic = 20.0f, .thermal = 10.0f, .brisance = 30.0f, });
        const locality::geo::GeneralizedRecipe iceAsteroid{
            .mix = locality::geo::GeneralizedRecipe::homogenous(0), // Ice
            .radius = 50.0f,
            .lump = 0.05f, // high sphericity, not a perfect ball
            .seed = 77,
            .spotMeters = 16.0f,
            .spotContrast = 0.5f,
        };
        const auto asteroid = with<locality::geo::Rock>::spawnGenerated(context, device, Pose::from(Pos{0.0f, -60.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), iceAsteroid, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        with<phys::rigid::CelestialGravity>::extend(context, with<locality::geo::Rock>::get(context, asteroid).body, phys::rigid::CelestialGravity::Quantum{.averageRadius = iceAsteroid.radius, .surfaceAcceleration = 2.0f});
    }

}
