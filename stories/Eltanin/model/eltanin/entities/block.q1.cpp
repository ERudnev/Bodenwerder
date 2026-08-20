#include <eltanin/entities/block.q1.h>
#include <rmmr/scene/node.q1.h>

#include <base/logging.h>

namespace eltanin {
    using namespace fqsm::api;

    namespace {

        auto localsFromCornerIndices(const vector<mech::cube::Corner>& indices) -> vector<vec3> {
            vector<vec3> locals;
            locals.reserve(indices.size());
            for (const mech::cube::Corner corner : indices) {
                locals.push_back(mech::space::cell::cell2local(mech::cube::corners[static_cast<std::size_t>(corner)]));
            }
            return locals;
        }

        auto spawnWithLocals(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, vector<vec3> locals, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Block::Id {
            if (locals.empty()) {
                return context.refuse("eltanin::Block::spawn: rest locals empty");
            }

            vec3 restCom{0.0f, 0.0f, 0.0f};
            float massSum = 0.0f;
            for (const auto& local : locals) {
                constexpr float mass = 1.0f;
                restCom += local * mass;
                massSum += mass;
            }
            restCom /= massSum;

            vector<phys::Particle> particles;
            particles.reserve(locals.size());
            vector<vec3> shape;
            shape.reserve(locals.size());
            for (const auto& local : locals) {
                const vec3 world = pose.position + pose.rotation * local;
                particles.push_back(phys::Particle{phys::Matter{.position = world, .mass = 1.0f, .temperature = 0.0f, .cohesion = 1.0f}, world});
                shape.push_back(local);
            }

            const phys::Body restored = phys::rigid::restoredBody(pose, particles, shape);
            const auto body = with<phys::rigid::Crystal>::create(context, phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = restCom,
                .restored = restored,
            });
            with<phys::rigid::Horned>::extend(context, body, phys::rigid::Horned::Quantum{});
            const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, pose, std::move(actorQuantum), std::move(stateQuantum));
            return with<Block>::create(context, Block::Quantum{.body = body, .actor = actor});
        }

    } // namespace

    auto Block::Actions::spawnPlate(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, mech::plate::shape shape, mech::Role, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Id {
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::plate::perimeter.size()) {
            return context.refuse("eltanin::Block::spawnPlate: shape out of range");
        }
        return spawnWithLocals(context, root, pose, localsFromCornerIndices(mech::plate::perimeter[index]), std::move(actorQuantum), std::move(stateQuantum));
    }

    auto Block::Actions::spawnFrame(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, mech::frame::shape shape, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Id {
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::frame::corners.size()) {
            return context.refuse("eltanin::Block::spawnFrame: shape out of range");
        }
        return spawnWithLocals(context, root, pose, localsFromCornerIndices(mech::frame::corners[index]), std::move(actorQuantum), std::move(stateQuantum));
    }

    auto Block::Actions::spawnInner(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, mech::frame::shape shape, mech::Role role, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Id {
        stateQuantum.albedo = mech::settings::colorCode(role);
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::frame::corners.size()) {
            return context.refuse("eltanin::Block::spawnInner: shape out of range");
        }
        return spawnWithLocals(context, root, pose, localsFromCornerIndices(mech::frame::corners[index]), std::move(actorQuantum), std::move(stateQuantum));
    }

    struct Block::Internals : Block::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, block] : context.proposal.aspect<Block>().items()) {
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, block.actor)) { my::remove(context, id); continue; }
                const rmmr::Pose pose = body->restored.pose();
                if (not pose.near(with<rmmr::scene::Node>::get(context, block.actor).pose))
                    with<rmmr::scene::Node>::modify(context, block.actor)->pose = pose;
            }
        }
    };

    auto Block::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Block, phys::rigid::Crystal, &Block::Quantum::body>{},
            reaction::structural::custody<Block, rmmr::scene::actor::Mesh, &Block::Quantum::actor>{},
            reaction::aspect_wide<Block, phys::rigid::Crystal>(&Block::Internals::followBody),
        };
    }
}
