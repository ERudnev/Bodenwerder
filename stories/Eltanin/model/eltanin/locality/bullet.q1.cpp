#include <eltanin/locality/bullet.q1.h>

#include <eltanin/physics/body.q1.h>
#include "physics/settings.h"
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/family.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/geometric.hpp>

#include <cstddef>

namespace eltanin::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float shellMass = 0.40f;
        constexpr float shellRadius = 0.015f;
        constexpr float shellHeat = 3200.0f;

        auto composeShell30mm(Writing context) -> optional<scene::actor::Family::Quantum> {
            const auto familyMaterial = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("rmmr", "familyTracer"));
            if (not familyMaterial)
                return {};
            const auto pack = with<resource::Assets>::find<resource::meshpack::Asset>(context, resource::Unit::Name::from("Eltanin", "projectiles"));
            if (not pack)
                return {};
            auto resolved = with<resource::meshpack::Asset>::resolve(context, *pack, "shell_30mm");
            if (not resolved)
                return {};
            for (auto& entry : resolved->surfaces)
                entry.second = resource::material::Instance{.material = *familyMaterial, .textures = {}};
            resolved->texpack = {};
            using Type = resource::Uniform::Type;
            return with<scene::actor::Family>::compose(context, std::move(*resolved), scene::actor::Family::Layout{
                .instanceBytes = 16,
                .fields = {
                    scene::actor::Family::Field{.name = "speed", .type = Type::f32, .offset = 0},
                    scene::actor::Family::Field{.name = "heat", .type = Type::f32, .offset = 4},
                },
            });
        }

    } // namespace

    void Bullet::Actions::bindResources(Writing context) {
        if (with<Bullet>::get_global(context).resources)
            return;
        const auto scene = with<Thing>::get_global(context).scene;
        auto familyQuantum = composeShell30mm(context);
        if (not familyQuantum) {
            context.refuse("eltanin::locality::Bullet::bindResources: shell_30mm family compose failed");
            return;
        }
        with<Bullet>::modify_global(context)->resources = Resources{.shell30mm = with<scene::Interface>::createFamily(context, scene, std::move(*familyQuantum))};
    }

    auto Bullet::Actions::spawnShell30mm(Writing context, Pose pose, float speed) -> Id {
        const auto scene = with<Thing>::get_global(context).scene;
        const auto& resources = with<Bullet>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::locality::Bullet::spawnShell30mm: resources not bound");
        const auto family = resources->shell30mm;
        const auto bytes = with<scene::actor::Family>::get(context, family).layout.instanceBytes;
        if (bytes < 0)
            return context.refuse("eltanin::locality::Bullet::spawnShell30mm: Family.layout.instanceBytes is negative");
        scene::actor::Packed packed(static_cast<std::size_t>(bytes));
        with<scene::actor::Family>::write(context, family, packed, "speed", speed);
        with<scene::actor::Family>::write(context, family, packed, "heat", shellHeat);
        const auto replica = with<scene::Interface>::createReplica(context, scene, family, pose, scene::actor::Replica::Quantum{.family = family, .packed = std::move(packed)});
        if (not with<scene::actor::Replica>::exists(context, replica))
            return context.refuse("eltanin::locality::Bullet::spawnShell30mm: replica create failed");
        const vec3 nose = pose.rotation * vec3{0.0f, 0.0f, -1.0f};
        const vec3 velocity = nose * speed;
        const auto body = with<phys::Body>::create(context, phys::Body::Quantum{
            .position = dvec3{pose.position},
            .orientation = pose.rotation,
            .totalMass = shellMass,
            .radius = shellRadius,
        });
        with<phys::rigid::Ray>::extend(context, body, phys::rigid::Ray::Quantum{
            .core = phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = shellMass, .temperature = shellHeat, .cohesion = 1.0f}, dvec3{pose.position} - dvec3{velocity * float(phys::Settings::fixedStep)}, vec3{0.0f, 0.0f, 0.0f}},
        });
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Bullet>::extend(context, thing, Bullet::Quantum{.actor = replica, .body = body, .speed = speed});
        return thing;
    }

    void Bullet::Actions::update(Writing context) {
        constexpr seconds lifetime = 5.0;
        const seconds now = with<Thing>::get_global(context).now;
        vector<Id> expired;
        for (auto [id, _] : context->aspect<Bullet>().items()) {
            if (not with<Thing>::exists(context, id))
                continue;
            if (now - with<Thing>::get(context, id).bornAt >= lifetime)
                expired.push_back(id);
        }
        for (const auto id : expired)
            with<Bullet>::kraken(context, id);
    }

    void Bullet::Actions::followBody(Stewarding context) {
        auto nodes = context.direct<scene::Node>();
        auto bodies = context.direct<phys::Body>();
        auto rays = context.direct<phys::rigid::Ray>();
        auto replicas = context.direct<scene::actor::Replica>();
        for (auto [_, bullet] : context->aspect<Bullet>().items()) {
            auto* node = nodes.items.find(bullet.actor);
            if (not node)
                continue;
            auto* body = bodies.items.find(bullet.body);
            if (not body)
                continue;
            node->pose = body->pose();
            auto* ray = rays.items.find(bullet.body);
            auto* replica = replicas.items.find(bullet.actor);
            if (not ray or not replica)
                continue;
            with<scene::actor::Family>::write(context, replica->family, replica->packed, "speed", float(glm::length((ray->core.position - ray->core.prev) / phys::Settings::fixedStep)));
        }
    }

    auto Bullet::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Bullet, scene::actor::Replica, &Bullet::Quantum::actor>{},
            reaction::structural::custody<Bullet, phys::rigid::Ray, &Bullet::Quantum::body>{},
        };
    }

}
