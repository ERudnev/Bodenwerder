#include <eltanin/locality/flash.q1.h>

#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace eltanin::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto falloff(float distance) -> float {
            return 1.0f / (distance * distance + 1.0f);
        }

        auto frontPasses(float distance, float radius, float inner, float outer) -> bool {
            if (radius <= 0.0f)
                return false;
            return distance >= radius * inner and distance < radius * outer;
        }

        void strikeParticle(phys::Particle& particle, vec3 origin, float inner, float outer, const Flash::Effect& effect) {
            if (particle.mass <= 0.0f)
                return;
            const vec3 offset = vec3{particle.position} - origin;
            const float distance = glm::length(offset);
            const float atten = falloff(distance);
            if (frontPasses(distance, effect.kinetic.radius, inner, outer) and distance > 1.0e-4f)
                particle.force += (offset / distance) * (effect.kinetic.strength * atten);
            if (frontPasses(distance, effect.thermal.radius, inner, outer))
                particle.temperature = glm::max(particle.temperature, glm::min(effect.thermal.temperature, particle.temperature + effect.thermal.energy * atten / particle.mass));
            if (frontPasses(distance, effect.fracture.radius, inner, outer))
                particle.cohesion -= effect.fracture.yield * atten;
        }

        void strikeSolid(phys::rigid::Solid::Quantum& solid, const phys::Body::Quantum& body, vec3 origin, float inner, float outer, const Flash::Effect& effect) {
            if (body.totalMass <= 0.0f)
                return;
            const vec3 offset = vec3{body.position} - origin;
            const float distance = glm::length(offset);
            const float atten = falloff(distance);
            if (frontPasses(distance, effect.kinetic.radius, inner, outer) and distance > 1.0e-4f)
                solid.center.force += (offset / distance) * (effect.kinetic.strength * atten);
            if (frontPasses(distance, effect.thermal.radius, inner, outer))
                solid.center.temperature = glm::max(solid.center.temperature, glm::min(effect.thermal.temperature, solid.center.temperature + effect.thermal.energy * atten / body.totalMass));
            if (frontPasses(distance, effect.fracture.radius, inner, outer))
                solid.center.cohesion -= effect.fracture.yield * atten;
        }

        auto lookOf(const Flash::Effect& effect, seconds elapsed) -> scene::actor::MeshState::Quantum {
            const float duration = float(effect.duration);
            const float progress = duration > 0.0f ? glm::clamp(float(elapsed) / duration, 0.0f, 1.0f) : 1.0f;
            const float reach = glm::max(glm::max(effect.kinetic.radius, effect.thermal.radius), effect.fracture.radius);
            const float radius = glm::max(reach * glm::max(progress, 0.02f), 0.4f);
            const float fade = 1.0f - progress;
            auto state = with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, fade * fade, vec3{radius, radius, radius});
            state.heat.x = effect.thermal.temperature;
            return state;
        }

        void paintActor(Writing context, scene::actor::Mesh::Id actor, const Flash::Effect& effect, seconds elapsed) {
            if (not with<scene::actor::MeshState>::exists(context, actor))
                return;
            const auto look = lookOf(effect, elapsed);
            auto state = with<scene::actor::MeshState>::modify(context, actor);
            state->scale = look.scale;
            state->opacity = look.opacity;
            state->heat.x = look.heat.x;
        }

    }

    auto Flash::Actions::spawnAsExplosion(Writing context, vec3 position, float strength) -> Id {
        if (strength <= 0.0f)
            return context.refuse("eltanin::locality::Flash::spawnAsExplosion: strength must be positive");
        const auto scene = with<Thing>::get_global(context).scene;
        const auto sphere = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("rmmr", "sphere"));
        if (not sphere)
            return context.refuse("eltanin::locality::Flash::spawnAsExplosion: sphere geometry missing");
        const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "flash"));
        if (not material)
            return context.refuse("eltanin::locality::Flash::spawnAsExplosion: flash material missing");
        auto meshQuantum = with<scene::actor::Mesh>::composeOne(context, *sphere, *material);
        if (not meshQuantum)
            return context.refuse("eltanin::locality::Flash::spawnAsExplosion: mesh compose failed");
        const float radius = 12.0f * strength;
        const Effect effect{
            .duration = 0.70,
            .kinetic = {.strength = 5.0e7f * strength, .radius = radius},
            .thermal = {.temperature = 2800.0f + 200.0f * strength, .energy = 8.0e6f * strength, .radius = radius * 1.15f},
            .fracture = {.yield = 8.0f * strength, .radius = radius * 0.85f},
        };
        const auto actor = with<scene::Interface>::createMeshActor(context, scene, Pose::from(position, HPB{0.0f, 0.0f, 0.0f}), std::move(*meshQuantum), lookOf(effect, seconds{}));
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Flash>::extend(context, thing, Flash::Quantum{.effect = effect, .actor = actor, .elapsed = seconds{}});
        return thing;
    }

    void Flash::Actions::update(Writing context) {
        vector<Id> expired;
        for (auto [id, flash] : context->aspect<Flash>().items()) {
            if (flash.effect.duration <= 0 or flash.elapsed >= flash.effect.duration) {
                expired.push_back(id);
                continue;
            }
            paintActor(context, flash.actor, flash.effect, flash.elapsed);
        }
        for (const auto id : expired)
            with<Flash>::kraken(context, id);
    }

    void Flash::Actions::apply(Stewarding context) {
        auto nodes = context.direct<scene::Node>();
        auto crystals = context.direct<phys::rigid::Crystal>();
        auto solids = context.direct<phys::rigid::Solid>();
        auto bodies = context.direct<phys::Body>();
        const float tick = phys::Particle::dt;
        for (auto [_, flash] : context.direct<Flash>().items) {
            if (flash.effect.duration <= 0)
                continue;
            const seconds age = flash.elapsed;
            if (age >= flash.effect.duration)
                continue;
            flash.elapsed += tick;
            const float span = float(flash.effect.duration);
            const float inner = glm::clamp(float(age) / span, 0.0f, 1.0f);
            const float outer = glm::clamp(float(flash.elapsed) / span, 0.0f, 1.0f);
            auto* node = nodes.items.find(flash.actor);
            if (not node)
                continue;
            const vec3 origin = node->pose.position;
            for (auto [_, crystal] : crystals.items) {
                for (phys::Particle& particle : crystal.particles)
                    strikeParticle(particle, origin, inner, outer, flash.effect);
            }
            for (auto [bodyId, solid] : solids.items) {
                auto* body = bodies.items.find(bodyId);
                if (not body)
                    continue;
                strikeSolid(solid, *body, origin, inner, outer, flash.effect);
            }
        }
    }

    auto Flash::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Flash, scene::actor::Mesh, &Flash::Quantum::actor>{},
        };
    }

}
