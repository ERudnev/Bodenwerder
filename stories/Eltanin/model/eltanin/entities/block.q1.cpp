#include <eltanin/entities/block.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/scene/node.q1.h>

namespace eltanin {
    using namespace fqsm::api;

    auto Block::Actions::spawn(Writing context, rmmr::scene::Root::Id root, resource::atomic::Asset::Id shape, rmmr::Locator locator, rmmr::scene::actor::Simple::Quantum actor_quantum) -> Id {
        const auto pose = rmmr::Pose::from(locator);
        const auto& asset = with<resource::atomic::Asset>::get(context, shape);
        vector<phys::Particle::Id> particles; particles.reserve(asset.points.size());
        for (const auto& local : asset.points) {
            const vec3 world = pose.position + pose.rotation * local;
            particles.push_back(with<phys::Particle>::create(context, phys::Particle::Quantum{.current = world, .prev = world, .mass = 1.0f}));
        }
        const auto body = with<phys::Atomic>::create(context, phys::Atomic::Quantum{.particles = std::move(particles), .restored = pose, .shape = shape});
        const auto actor = with<rmmr::scene::Interface>::createSimpleActor(context, root, locator, std::move(actor_quantum));
        return with<Block>::create(context, Block::Quantum{.body = body, .actor = actor});
    }

    struct Block::Internals : Block::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, block] : context.proposal.aspect<Block>().items()) {
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, block.actor)) { my::remove(context, id); continue; }
                if (not body->restored.near(with<rmmr::scene::Node>::get(context, block.actor).pose))
                    with<rmmr::scene::Node>::modify(context, block.actor)->pose = body->restored;
            }
        }
    };

    auto Block::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Block, phys::Atomic, &Block::Quantum::body>{},
            reaction::structural::custody<Block, rmmr::scene::actor::Simple, &Block::Quantum::actor>{},
            reaction::aspect_wide<Block, phys::Atomic>(&Block::Internals::followBody),
        };
    }
}
