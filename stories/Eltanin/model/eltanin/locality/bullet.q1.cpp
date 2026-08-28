#include <eltanin/locality/bullet.q1.h>

#include <eltanin/physics/body.q1.h>
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

    void Bullet::Actions::bind(Writing context, scene::Root::Id root) {
        if (with<Bullet>::get_global(context).shell30mm) {
            context.refuse("eltanin::locality::Bullet::bind: already bound");
            return;
        }
        auto familyQuantum = composeShell30mm(context);
        if (not familyQuantum) {
            context.refuse("eltanin::locality::Bullet::bind: shell_30mm family compose failed");
            return;
        }
        with<Bullet>::modify_global(context)->shell30mm = with<scene::Interface>::createFamily(context, root, std::move(*familyQuantum));
    }

    auto Bullet::Actions::spawnShell30mm(Writing context, scene::Root::Id root, Pose pose, float speed) -> Id {
        const auto family = with<Bullet>::get_global(context).shell30mm;
        if (not family)
            return context.refuse("eltanin::locality::Bullet::spawnShell30mm: class is not bound");
        const auto bytes = with<scene::actor::Family>::get(context, *family).layout.instanceBytes;
        if (bytes < 0)
            return context.refuse("eltanin::locality::Bullet::spawnShell30mm: Family.layout.instanceBytes is negative");
        scene::actor::Packed packed(static_cast<std::size_t>(bytes));
        with<scene::actor::Family>::write(context, *family, packed, "speed", speed);
        with<scene::actor::Family>::write(context, *family, packed, "heat", shellHeat);
        const auto replica = with<scene::Interface>::createReplica(context, root, *family, pose, scene::actor::Replica::Quantum{.family = *family, .packed = std::move(packed)});
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
            .core = phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = shellMass, .temperature = shellHeat, .cohesion = 1.0f}, dvec3{pose.position} - dvec3{velocity * phys::Particle::dt}, vec3{0.0f, 0.0f, 0.0f}},
        });
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Bullet>::extend(context, thing, Bullet::Quantum{.actor = replica, .body = body, .speed = speed});
        return thing;
    }

    void Bullet::Actions::update(Stewarding context) {
        constexpr int64 lifetimeUs = 5'000'000;
        auto things = context.direct<Thing>();
        const int64 now = things.global.now;
        vector<Id> expired;
        for (auto [id, _] : context.direct<Bullet>().items) {
            const auto* thing = things.items.find(id);
            if (not thing)
                continue;
            if (now - thing->bornAt >= lifetimeUs)
                expired.push_back(id);

        }
        for (const auto id : expired)
            with<Bullet>::kraken(context, id);
    }

    struct Bullet::Internals : Bullet::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, bullet] : context.proposal.aspect<Bullet>().items()) {
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, bullet.actor)) { my::remove(context, id); continue; }
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, bullet.actor)->pose = body->pose();
                if (not with<phys::rigid::Ray>::exists(context, bullet.body) or not with<scene::actor::Replica>::exists(context, bullet.actor))
                    continue;
                const float speed = float(glm::length(with<phys::rigid::Ray>::get(context, bullet.body).core.velocity()));
                auto replica = with<scene::actor::Replica>::modify(context, bullet.actor);
                with<scene::actor::Family>::write(context, replica->family, replica->packed, "speed", speed);
            }
        }
    };

    auto Bullet::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Bullet, scene::actor::Replica, &Bullet::Quantum::actor>{},
            reaction::structural::custody<Bullet, phys::rigid::Ray, &Bullet::Quantum::body>{},
            reaction::aspect_wide<Bullet, phys::Body>(&Bullet::Internals::followBody),
        };
    }

}
