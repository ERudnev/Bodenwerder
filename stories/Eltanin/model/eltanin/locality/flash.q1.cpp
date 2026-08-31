#include <eltanin/locality/flash.q1.h>

#include <eltanin/physics/rigid.q1.h>
#include "physics/settings.h"
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto falloff(float distance) -> float {
            return 1.0f / (distance * distance + 1.0f);
        }

        constexpr float thermalHalo = 2.5f; // heat soaks this many plasma radii; visual ball stays thermal.radius
        constexpr RGB brisanceTint{0.22f, 0.72f, 0.38f};
        constexpr float brisanceFade = 0.10f;

        auto thermalSoak(float distance, float radius) -> float {
            if (radius <= 0.0f)
                return 0.0f;
            const float u = distance / radius;
            if (u >= thermalHalo)
                return 0.0f;
            return 1.0f / (1.0f + u * u);
        }

        auto frontPasses(float distance, float radius, float inner, float outer) -> bool {
            if (radius <= 0.0f)
                return false;
            return distance >= radius * inner and distance < radius * outer;
        }

        auto thermalPulse(seconds elapsed, seconds duration) -> float {
            if (duration <= 0)
                return 0.0f;
            const float progress = glm::clamp(float(elapsed) / float(duration), 0.0f, 1.0f);
            const float wave = std::sin(3.14159265f * progress);
            return wave * wave;
        }

        auto waveSpan(seconds elapsed, seconds duration, float& inner, float& outer) -> void {
            const float span = float(duration);
            if (span <= 0.0f) {
                inner = 1.0f;
                outer = 1.0f;
                return;
            }
            inner = glm::clamp(float(elapsed) / span, 0.0f, 1.0f);
            outer = glm::clamp((float(elapsed) + float(phys::Settings::fixedStep)) / span, 0.0f, 1.0f);
        }

        auto flashLife(const Flash::Effect& effect) -> seconds {
            seconds life{};
            if (effect.kinetic.radius > 0.0f)
                life = effect.kinetic.duration;
            if (effect.thermal.radius > 0.0f and effect.thermal.duration > life)
                life = effect.thermal.duration;
            if (effect.brisance.radius > 0.0f and effect.brisance.duration > life)
                life = effect.brisance.duration;
            return life;
        }

        auto ambientKelvin(Reading context) -> float {
            return with<scene::Root>::get(context, with<Thing>::get_global(context).scene).atmosphereTemperature;
        }

        void strikeParticle(phys::Particle& particle, vec3 origin, float kineticInner, float kineticOuter, float brisanceInner, float brisanceOuter, float pulse, float ambient, const Flash::Effect& effect) {
            if (particle.mass <= 0.0f)
                return;
            const vec3 offset = vec3{particle.position} - origin;
            const float distance = glm::length(offset);
            const float atten = falloff(distance);
            if (frontPasses(distance, effect.kinetic.radius, kineticInner, kineticOuter) and distance > 1.0e-4f)
                particle.force += (offset / distance) * (effect.kinetic.strength * atten);
            const float soak = thermalSoak(distance, effect.thermal.radius);
            if (soak > 0.0f)
                particle.temperature = glm::max(particle.temperature, ambient + (effect.thermal.temperature - ambient) * pulse * soak);
            if (frontPasses(distance, effect.brisance.radius, brisanceInner, brisanceOuter))
                particle.cohesion -= effect.brisance.yield * atten;
        }

        void strikeSolid(phys::rigid::Solid::Quantum& solid, const phys::Body::Quantum& body, vec3 origin, float kineticInner, float kineticOuter, float brisanceInner, float brisanceOuter, float pulse, float ambient, const Flash::Effect& effect) {
            if (body.totalMass <= 0.0f)
                return;
            const vec3 offset = vec3{body.position} - origin;
            const float distance = glm::length(offset);
            const float atten = falloff(distance);
            if (frontPasses(distance, effect.kinetic.radius, kineticInner, kineticOuter) and distance > 1.0e-4f)
                solid.center.force += (offset / distance) * (effect.kinetic.strength * atten);
            const float soak = thermalSoak(distance, effect.thermal.radius);
            if (soak > 0.0f)
                solid.center.temperature = glm::max(solid.center.temperature, ambient + (effect.thermal.temperature - ambient) * pulse * soak);
            if (frontPasses(distance, effect.brisance.radius, brisanceInner, brisanceOuter))
                solid.center.cohesion -= effect.brisance.yield * atten;
        }

        auto lookShock(const Flash::Effect& effect, seconds elapsed) -> scene::actor::MeshState::Quantum {
            const float duration = float(effect.kinetic.duration);
            const float progress = duration > 0.0f ? glm::clamp(float(elapsed) / duration, 0.0f, 1.0f) : 1.0f;
            const float radius = glm::mix(effect.kinetic.core, effect.kinetic.radius, progress);
            const float fade = 1.0f - progress;
            auto state = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, fade * fade, vec3{radius, radius, radius});
            state.heat.x = 6500.0f;
            return state;
        }

        auto lookPlasma(const Flash::Effect& effect, seconds elapsed, float ambient) -> scene::actor::MeshState::Quantum {
            const float pulse = thermalPulse(elapsed, effect.thermal.duration);
            const float radius = effect.thermal.radius;
            auto state = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, pulse, vec3{radius, radius, radius});
            state.heat.x = ambient + (effect.thermal.temperature - ambient) * pulse;
            return state;
        }

        auto lookField(const Flash::Effect& effect, seconds elapsed) -> scene::actor::MeshState::Quantum {
            const float duration = float(effect.brisance.duration);
            const float progress = duration > 0.0f ? glm::clamp(float(elapsed) / duration, 0.0f, 1.0f) : 1.0f;
            const float radius = effect.brisance.radius;
            return with<scene::actor::MeshState>::defaults(brisanceTint, brisanceFade * (1.0f - progress), vec3{radius, radius, radius});
        }

        auto hiddenLook() -> scene::actor::MeshState::Quantum {
            return with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 0.0f, vec3{0.0f, 0.0f, 0.0f});
        }

        void paintActor(Writing context, scene::actor::Mesh::Id actor, const scene::actor::MeshState::Quantum& look) {
            if (not with<scene::actor::MeshState>::exists(context, actor))
                return;
            auto state = with<scene::actor::MeshState>::modify(context, actor);
            state->albedo = look.albedo;
            state->scale = look.scale;
            state->opacity = look.opacity;
            state->heat.x = look.heat.x;
        }

        auto flashWarhead(float strength) -> Flash::Effect {
            const float radius = 12.0f * strength;
            return Flash::Effect{
                .kinetic = {.strength = 5.0e7f * strength, .radius = radius, .core = 0.01f * strength, .duration = 0.70},
                .thermal = {.temperature = 0.0f, .energy = 0.0f, .radius = 0.0f, .duration = seconds{}},
                .brisance = {.yield = 0.0f, .radius = 0.0f, .duration = seconds{}},
            };
        }

        auto flashPlasma(float strength) -> Flash::Effect {
            return Flash::Effect{
                .kinetic = {.strength = 0.0f, .radius = 0.0f, .core = 0.0f, .duration = seconds{}},
                .thermal = {.temperature = 2800.0f + 200.0f * strength, .energy = 0.0f, .radius = 1.0f * strength, .duration = 1.0},
                .brisance = {.yield = 0.0f, .radius = 0.0f, .duration = seconds{}},
            };
        }

        auto flashBrisance(float strength) -> Flash::Effect {
            return Flash::Effect{
                .kinetic = {.strength = 0.0f, .radius = 0.0f, .core = 0.0f, .duration = seconds{}},
                .thermal = {.temperature = 0.0f, .energy = 0.0f, .radius = 0.0f, .duration = seconds{}},
                .brisance = {.yield = 8.0f * strength, .radius = 12.0f * strength * 0.85f, .duration = 0.20},
            };
        }

        auto spawnSphere(Writing context, scene::Root::Id scene, vec3 position, resource::geometry::Asset::Id sphere, resource::material::Asset::Id material, scene::actor::MeshState::Quantum look) -> scene::actor::Mesh::Id {
            auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, sphere, material);
            if (not meshQuantum)
                return context.refuse("eltanin::locality::Flash::spawn: mesh compose failed");
            return with<scene::Interface>::createMeshActor(context, scene, Pose::from(position, HPB{0.0f, 0.0f, 0.0f}), std::move(*meshQuantum), look);
        }

        auto spawnFlash(Writing context, vec3 position, vec3 linear, Flash::Effect effect) -> Flash::Id {
            const auto& resources = with<Flash>::get_global(context).resources;
            if (not resources)
                return context.refuse("eltanin::locality::Flash::spawn: resources not bound");
            const auto scene = with<Thing>::get_global(context).scene;
            const float ambient = with<scene::Root>::get(context, scene).atmosphereTemperature;
            const auto shockLook = effect.kinetic.radius > 0.0f ? lookShock(effect, seconds{}) : hiddenLook();
            const auto plasmaLook = effect.thermal.radius > 0.0f ? lookPlasma(effect, seconds{}, ambient) : hiddenLook();
            const auto fieldLook = effect.brisance.radius > 0.0f ? lookField(effect, seconds{}) : hiddenLook();
            const auto shock = spawnSphere(context, scene, position, resources->sphere, resources->flash, shockLook);
            const auto plasma = spawnSphere(context, scene, position, resources->sphere, resources->flashGlow, plasmaLook);
            const auto field = spawnSphere(context, scene, position, resources->sphere, resources->brisance, fieldLook);
            const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
            with<Flash>::extend(context, thing, Flash::Quantum{.effect = effect, .shock = shock, .plasma = plasma, .field = field, .linear = linear, .elapsed = seconds{}});
            return thing;
        }

    }

    void Flash::Actions::bindResources(Writing context) {
        if (with<Flash>::get_global(context).resources)
            return;
        const auto sphere = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "flashSphere"));
        if (not sphere) {
            context.refuse("eltanin::locality::Flash::bindResources: flashSphere geometry missing");
            return;
        }
        const auto flash = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "flash"));
        if (not flash) {
            context.refuse("eltanin::locality::Flash::bindResources: flash material missing");
            return;
        }
        const auto flashGlow = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "flashGlow"));
        if (not flashGlow) {
            context.refuse("eltanin::locality::Flash::bindResources: flashGlow material missing");
            return;
        }
        const auto brisance = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "flashBrisance"));
        if (not brisance) {
            context.refuse("eltanin::locality::Flash::bindResources: flashBrisance material missing");
            return;
        }
        with<Flash>::modify_global(context)->resources = Resources{.sphere = *sphere, .flash = *flash, .flashGlow = *flashGlow, .brisance = *brisance};
    }

    auto Flash::Actions::spawnAsExplosion(Writing context, vec3 position, vec3 linear, float strength) -> Id {
        if (strength <= 0.0f)
            return context.refuse("eltanin::locality::Flash::spawnAsExplosion: strength must be positive");
        return spawnFlash(context, position, linear, flashWarhead(strength));
    }

    auto Flash::Actions::spawnAsThermal(Writing context, vec3 position, vec3 linear, float strength) -> Id {
        if (strength <= 0.0f)
            return context.refuse("eltanin::locality::Flash::spawnAsThermal: strength must be positive");
        return spawnFlash(context, position, linear, flashPlasma(strength));
    }

    auto Flash::Actions::spawnAsBrisance(Writing context, vec3 position, vec3 linear, float strength) -> Id {
        if (strength <= 0.0f)
            return context.refuse("eltanin::locality::Flash::spawnAsBrisance: strength must be positive");
        return spawnFlash(context, position, linear, flashBrisance(strength));
    }

    void Flash::Actions::update(Writing context) {
        const float ambient = ambientKelvin(context);
        vector<Id> expired;
        for (auto [id, flash] : context->aspect<Flash>().items()) {
            const seconds life = flashLife(flash.effect);
            if (life <= 0 or flash.elapsed >= life) {
                expired.push_back(id);
                continue;
            }
            if (flash.effect.kinetic.radius > 0.0f)
                paintActor(context, flash.shock, lookShock(flash.effect, flash.elapsed));
            if (flash.effect.thermal.radius > 0.0f)
                paintActor(context, flash.plasma, lookPlasma(flash.effect, flash.elapsed, ambient));
            if (flash.effect.brisance.radius > 0.0f)
                paintActor(context, flash.field, lookField(flash.effect, flash.elapsed));
        }
        for (const auto id : expired)
            with<Flash>::kraken(context, id);
    }

    void Flash::Actions::apply(Stewarding context) {
        auto nodes = context.direct<scene::Node>();
        auto crystals = context.direct<phys::rigid::Crystal>();
        auto solids = context.direct<phys::rigid::Solid>();
        auto bodies = context.direct<phys::Body>();
        const float tick = float(phys::Settings::fixedStep);
        const float ambient = ambientKelvin(context);
        for (auto [_, flash] : context.direct<Flash>().items) {
            const seconds life = flashLife(flash.effect);
            if (life <= 0)
                continue;
            const seconds age = flash.elapsed;
            if (age >= life)
                continue;
            float kineticInner = 1.0f;
            float kineticOuter = 1.0f;
            float brisanceInner = 1.0f;
            float brisanceOuter = 1.0f;
            waveSpan(age, flash.effect.kinetic.duration, kineticInner, kineticOuter);
            waveSpan(age, flash.effect.brisance.duration, brisanceInner, brisanceOuter);
            flash.elapsed += tick;
            const float pulse = thermalPulse(age, flash.effect.thermal.duration);
            const vec3 drift = flash.linear * tick;
            auto* shockNode = nodes.items.find(flash.shock);
            auto* plasmaNode = nodes.items.find(flash.plasma);
            auto* fieldNode = nodes.items.find(flash.field);
            if (shockNode)
                shockNode->pose.position += drift;
            if (plasmaNode)
                plasmaNode->pose.position += drift;
            if (fieldNode)
                fieldNode->pose.position += drift;
            auto* node = shockNode ? shockNode : (plasmaNode ? plasmaNode : fieldNode);
            if (not node)
                continue;
            const vec3 origin = node->pose.position;
            for (auto [_, crystal] : crystals.items) {
                for (phys::Particle& particle : crystal.particles)
                    strikeParticle(particle, origin, kineticInner, kineticOuter, brisanceInner, brisanceOuter, pulse, ambient, flash.effect);
            }
            for (auto [bodyId, solid] : solids.items) {
                auto* body = bodies.items.find(bodyId);
                if (not body)
                    continue;
                strikeSolid(solid, *body, origin, kineticInner, kineticOuter, brisanceInner, brisanceOuter, pulse, ambient, flash.effect);
            }
        }
    }

    auto Flash::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Flash, scene::actor::Mesh, &Flash::Quantum::shock>{},
            reaction::structural::custody<Flash, scene::actor::Mesh, &Flash::Quantum::plasma>{},
            reaction::structural::custody<Flash, scene::actor::Mesh, &Flash::Quantum::field>{},
        };
    }

}
