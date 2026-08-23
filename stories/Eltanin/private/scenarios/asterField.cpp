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
#include <cstdint>
#include <numbers>
#include <random>

#include <glm/geometric.hpp>

namespace eltanin::scenario {

    using namespace rmmr;

    namespace {

        constexpr float shellRadius = 10.0f;
        constexpr float pebbleRadius = 2.0f;
        constexpr float radialSpeed = 100.0f;
        constexpr float cloudSpacing = 30.0f;
        constexpr float spinRate = 2.0f * std::numbers::pi_v<float>; // 1 rev/s
        constexpr integer feldspar = 3;

        auto geodesicIcosaVertices() -> vector<vec3> {
            const float phi = 0.5f * (1.0f + std::sqrt(5.0f));
            vector<vec3> verts{
                {0.0f, -1.0f, -phi}, {0.0f, -1.0f, phi}, {0.0f, 1.0f, -phi}, {0.0f, 1.0f, phi},
                {-1.0f, -phi, 0.0f}, {-1.0f, phi, 0.0f}, {1.0f, -phi, 0.0f}, {1.0f, phi, 0.0f},
                {-phi, 0.0f, -1.0f}, {-phi, 0.0f, 1.0f}, {phi, 0.0f, -1.0f}, {phi, 0.0f, 1.0f},
            };
            for (vec3& vert : verts)
                vert = glm::normalize(vert);
            const auto icosaCount = verts.size();
            for (size_t i = 0; i < icosaCount; ++i)
                for (size_t j = i + 1; j < icosaCount; ++j)
                    if (glm::dot(verts[i], verts[j]) > 0.4f)
                        verts.push_back(glm::normalize(verts[i] + verts[j]));
            return verts;
        }

        auto randomUnitAxis(std::mt19937& rng) -> vec3 {
            std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
            vec3 axis{unit(rng), unit(rng), unit(rng)};
            const float length2 = glm::dot(axis, axis);
            if (length2 <= 1.0e-12f)
                return vec3{0.0f, 1.0f, 0.0f};
            return axis / std::sqrt(length2);
        }

        void spawnCloud(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, vec3 origin, const geo::Rock::GeneralizedRecipe& recipe, float outboundSpeed, vec3 bodyOmega, bool randomSpin, uint32_t spinSeed) {
            std::mt19937 spinRng(spinSeed);
            for (const vec3& dir : geodesicIcosaVertices()) {
                const vec3 omega = randomSpin ? randomUnitAxis(spinRng) * spinRate : bodyOmega;
                with<geo::Rock>::spawnGenerated(context, root, device, Pose::from(Pos{origin + dir * shellRadius}, HPB{0.0f, 0.0f, 0.0f}), recipe, dir * outboundSpeed, omega);
            }
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
        with<scene::Root>::modify(context, root)->atmosphereDensity = 1225.0f;
        const geo::Rock::GeneralizedRecipe recipe{
            .mix = geo::Rock::GeneralizedRecipe::homogenous(feldspar),
            .radius = pebbleRadius,
            .lump = 0.55f,
            .seed = 8000,
            .spotMeters = pebbleRadius * 2.0f,
            .spotContrast = 0.0f,
        };
        spawnCloud(context, root, device, vec3{0.0f, 0.0f, 0.0f}, recipe, radialSpeed, vec3{0.0f, 0.0f, 0.0f}, false, 0);
        spawnCloud(context, root, device, vec3{-cloudSpacing, 0.0f, 0.0f}, recipe, radialSpeed, vec3{0.0f, 0.0f, 0.0f}, true, 9001);
        spawnCloud(context, root, device, vec3{cloudSpacing, 0.0f, 0.0f}, recipe, 0.0f, vec3{0.0f, 0.0f, 0.0f}, false, 0);
    }

}
