#include "scenarios/asterField.h"

#include <eltanin/locality/bullet.q1.h>
#include <eltanin/locality/geo/boulder.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/semantics/rendering.h>
#include <rmmr/semantics/uniform.h>

#include <random>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

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
        constexpr int pebbleCount = 10;
        constexpr float pebbleRadius = 0.35f;
        constexpr vec3 cloudCenter{-50.0f, 0.0f, 0.0f};
        constexpr float cloudRadius = 1.2f;
        for (int index = 0; index < pebbleCount; ++index) {
            const float golden = glm::pi<float>() * (3.0f - std::sqrt(5.0f));
            const float y = 1.0f - 2.0f * (static_cast<float>(index) + 0.5f) / static_cast<float>(pebbleCount);
            const float ring = std::sqrt(glm::max(0.0f, 1.0f - y * y));
            const float theta = golden * static_cast<float>(index);
            const vec3 onSphere{ring * std::cos(theta), y, ring * std::sin(theta)};
            const locality::geo::GeneralizedRecipe recipe{
                .mix = locality::geo::GeneralizedRecipe::homogenous(index % 4),
                .radius = pebbleRadius,
                .lump = 0.45f,
                .seed = 40 + index,
                .spotMeters = pebbleRadius * 2.0f,
                .spotContrast = 0.35f,
            };
            with<locality::geo::Boulder>::spawnGenerated(context, root, device, Pose::from(Pos{cloudCenter + cloudRadius * onSphere}, HPB{0.0f, 0.0f, 0.0f}), recipe, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        }
        constexpr int bulletCount = 1000;
        constexpr float bulletStep = 3.0f;
        constexpr float speed = 200.0f;
        constexpr float scatter = 0.50f;
        std::mt19937 rng{20260828};
        std::normal_distribution<float> gauss{0.0f, scatter / 3.0f};
        for (int i = 0; i < bulletCount; ++i) {
            vec3 offset{0.0f, gauss(rng), gauss(rng)};
            const float length = glm::length(offset);
            if (length > scatter)
                offset *= scatter / length;
            const vec3 position{-100.0f - bulletStep * static_cast<float>(i), 0.0f, 0.0f};
            with<locality::Bullet>::spawnShell30mm(context, root, Pose::from(Pos{position + offset}, HPB{90.0f, 0.0f, 0.0f}), speed);
        }
    }

}
