#include <eltanin/entities/block.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/scene/node.q1.h>

#include <base/logging.h>

namespace eltanin {
    using namespace fqsm::api;

    namespace {

        auto locals_from_corner_indices(const vector<int>& indices) -> vector<vec3> {
            vector<vec3> locals;
            locals.reserve(indices.size());
            for (const int index : indices) {
                locals.push_back(mech::cube::toLocal(mech::cube::corners[static_cast<std::size_t>(index)]));
            }
            return locals;
        }

        auto spawn_with_locals(Writing context, rmmr::scene::Root::Id root, rmmr::Locator locator, vector<vec3> locals, rmmr::scene::actor::Mesh::Quantum actor_quantum) -> Block::Id {
            if (locals.empty()) {
                return context.refuse("eltanin::Block::spawn: rest locals empty");
            }

            const auto pose = rmmr::Pose::from(locator);

            vector<phys::Particle::Id> ids;
            ids.reserve(locals.size());

            vec3 rest_com{0.0f, 0.0f, 0.0f};
            float mass_sum = 0.0f;
            for (const auto& local : locals) {
                constexpr float mass = 1.0f;
                const vec3 world = pose.position + pose.rotation * local;
                ids.push_back(with<phys::Particle>::create(context, phys::Particle::Quantum{.current = world, .prev = world, .mass = mass}));
                rest_com += local * mass;
                mass_sum += mass;
            }
            rest_com /= mass_sum;

            vector<vec3> rest_centered;
            rest_centered.reserve(locals.size());
            for (const auto& local : locals) {
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

    } // namespace

    auto Block::Actions::spawnHull(Writing context, rmmr::scene::Root::Id root, rmmr::Locator locator, mech::hull::shape shape, mech::slots::hull slot, rmmr::scene::actor::Mesh::Quantum actor_quantum) -> Id {
        if (slot == mech::slots::hull::forbidden) {
            return context.refuse("eltanin::Block::spawnHull: slot forbidden");
        }
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::hull::perimeter.size()) {
            return context.refuse("eltanin::Block::spawnHull: shape out of range");
        }
        return spawn_with_locals(context, root, locator, locals_from_corner_indices(mech::hull::perimeter[index]), std::move(actor_quantum));
    }

    auto Block::Actions::spawnFrame(Writing context, rmmr::scene::Root::Id root, rmmr::Locator locator, mech::frame::shape shape, mech::slots::frame slot, rmmr::scene::actor::Mesh::Quantum actor_quantum) -> Id {
        switch (slot) {
            case mech::slots::frame::regular:
                break;
        }
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::frame::corners.size()) {
            return context.refuse("eltanin::Block::spawnFrame: shape out of range");
        }
        return spawn_with_locals(context, root, locator, locals_from_corner_indices(mech::frame::corners[index]), std::move(actor_quantum));
    }

    auto Block::Actions::spawnInner(Writing context, rmmr::scene::Root::Id root, rmmr::Locator locator, mech::inner::shape shape, mech::slots::inner slot, rmmr::scene::actor::Mesh::Quantum actor_quantum) -> Id {
        if (slot == mech::slots::inner::forbidden) {
            return context.refuse("eltanin::Block::spawnInner: slot forbidden");
        }
        switch (shape) {
            case mech::inner::shape::full: {
                vector<vec3> locals;
                locals.reserve(mech::cube::corners.size());
                for (const auto& lattice : mech::cube::corners) {
                    locals.push_back(mech::cube::toLocal(lattice));
                }
                return spawn_with_locals(context, root, locator, std::move(locals), std::move(actor_quantum));
            }
            case mech::inner::shape::half:
            case mech::inner::shape::quarter:
            case mech::inner::shape::octa:
                _INCOMPLETE_;
        }
    }

    auto Block::Actions::spawnWing(Writing context, rmmr::scene::Root::Id root, rmmr::Locator locator, mech::wing::shape shape, mech::slots::wing slot, rmmr::scene::actor::Mesh::Quantum actor_quantum) -> Id {
        switch (slot) {
            case mech::slots::wing::radiance:
            case mech::slots::wing::agfe:
                break;
        }
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::wing::corners.size()) {
            return context.refuse("eltanin::Block::spawnWing: shape out of range");
        }
        return spawn_with_locals(context, root, locator, locals_from_corner_indices(mech::wing::corners[index]), std::move(actor_quantum));
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
