#include <eltanin/entities/block.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/scene/node.q1.h>

namespace eltanin {
    using namespace fqsm::api;

    auto Block::Actions::spawn(Writing context, rmmr::scene::Root::Id root, resource::atomic::Asset::Id shape, rmmr::Locator locator, rmmr::scene::actor::Mesh::Quantum actor_quantum) -> Id {
        // Asset points are already game meters (no normalize/scale). Pose is rigid only: R, t.
        const auto pose = rmmr::Pose::from(locator);
        const auto& asset = with<resource::atomic::Asset>::get(context, shape);
        if (asset.points.empty()) {
            return context.refuse("eltanin::Block::spawn: atomic shape has no points");
        }

        vector<phys::Particle::Id> ids;
        ids.reserve(asset.points.size());

        vec3 rest_com{0.0f, 0.0f, 0.0f};
        float mass_sum = 0.0f;
        for (const auto& local : asset.points) {
            constexpr float mass = 1.0f;
            const vec3 world = pose.position + pose.rotation * local;
            ids.push_back(with<phys::Particle>::create(context, phys::Particle::Quantum{.current = world, .prev = world, .mass = mass}));
            rest_com += local * mass;
            mass_sum += mass;
        }
        rest_com /= mass_sum;

        vector<vec3> rest_centered;
        rest_centered.reserve(asset.points.size());
        for (const auto& local : asset.points) {
            rest_centered.push_back(local - rest_com);
        }

        const auto body = with<phys::Atomic>::create(context, phys::Atomic::Quantum{
            .particles = std::move(ids),
            .rest = phys::Atomic::Rest{
                .centered = std::move(rest_centered),
                .com = rest_com,
            },
            .restored = pose,
        });
        const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, locator, std::move(actor_quantum));
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
            reaction::structural::custody<Block, rmmr::scene::actor::Mesh, &Block::Quantum::actor>{},
            reaction::aspect_wide<Block, phys::Atomic>(&Block::Internals::followBody),
        };
    }
}
