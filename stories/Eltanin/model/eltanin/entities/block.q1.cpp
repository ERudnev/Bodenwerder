#include <eltanin/entities/block.q1.h>
#include <eltanin/physics/compound.q1.h>
#include <rmmr/scene/node.q1.h>

#include <base/logging.h>

#include <glm/geometric.hpp>

#include <utility>

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

        auto triangleFace(integer a, integer b, integer c, const vector<vec3>& shape, vec3 inside) -> phys::rigid::Hull::Face {
            const vec3 ab = shape[static_cast<std::size_t>(b)] - shape[static_cast<std::size_t>(a)];
            const vec3 ac = shape[static_cast<std::size_t>(c)] - shape[static_cast<std::size_t>(a)];
            vec3 normal = glm::cross(ab, ac);
            const float mag = glm::length(normal);
            if (mag <= 1.0e-12f)
                return phys::rigid::Hull::Face{.points = {}, .normal = vec3{0.0f, 1.0f, 0.0f}, .thickness = 0.0f};
            normal /= mag;
            const vec3 centroid = (shape[static_cast<std::size_t>(a)] + shape[static_cast<std::size_t>(b)] + shape[static_cast<std::size_t>(c)]) / 3.0f;
            if (glm::dot(normal, centroid - inside) < 0.0f) {
                std::swap(b, c);
                normal = -normal;
            }
            const float planeGap = glm::dot(centroid - inside, normal);
            return phys::rigid::Hull::Face{.points = {a, b, c}, .normal = normal, .thickness = planeGap > 0.0f ? planeGap : 0.0f};
        }

        auto hullFromCorners(const vector<vec3>& locals, const vector<mech::cube::Corner>& corners) -> phys::rigid::Hull {
            integer at[8];
            for (integer& slot : at)
                slot = -1;
            for (std::size_t index = 0; index < corners.size() and index < locals.size(); ++index)
                at[static_cast<std::size_t>(corners[index])] = static_cast<integer>(index);
            vec3 inside{0.0f, 0.0f, 0.0f};
            for (const vec3& local : locals)
                inside += local;
            if (not locals.empty())
                inside /= static_cast<float>(locals.size());
            phys::rigid::Hull hull{.faces = {}};
            for (const auto& loop : mech::cube::faces) {
                if (loop.size() != 4)
                    continue;
                integer quad[4];
                bool complete = true;
                for (std::size_t index = 0; index < 4; ++index) {
                    const integer id = at[static_cast<std::size_t>(loop[index])];
                    if (id < 0) {
                        complete = false;
                        break;
                    }
                    quad[index] = id;
                }
                if (not complete)
                    continue;
                auto first = triangleFace(quad[0], quad[1], quad[2], locals, inside);
                auto second = triangleFace(quad[0], quad[2], quad[3], locals, inside);
                if (first.points.size() == 3)
                    hull.faces.push_back(std::move(first));
                if (second.points.size() == 3)
                    hull.faces.push_back(std::move(second));
            }
            if (hull.faces.empty() and locals.size() >= 3) {
                for (integer index = 1; index + 1 < static_cast<integer>(locals.size()); ++index) {
                    auto face = triangleFace(0, index, index + 1, locals, inside);
                    if (face.points.size() == 3)
                        hull.faces.push_back(std::move(face));
                }
            }
            return hull;
        }

        auto spawnWithLocals(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, const vector<mech::cube::Corner>& corners, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Block::Id {
            auto locals = localsFromCornerIndices(corners);
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
                particles.push_back(phys::Particle{phys::Matter{.position = dvec3{world}, .mass = 1.0f, .temperature = 0.0f, .cohesion = 1.0f}, dvec3{world}, vec3{0.0f, 0.0f, 0.0f}});
                shape.push_back(local);
            }

            auto hull = hullFromCorners(shape, corners);
            const auto body = with<phys::Body>::create(context, phys::rigid::restoredBody(pose, particles, shape));
            with<phys::rigid::Crystal>::extend(context, body, phys::rigid::Crystal::Quantum{
                .particles = std::move(particles),
                .shape = std::move(shape),
                .com = restCom,
                .hull = std::move(hull),
            });
            with<phys::Compound>::extend(context, body, phys::Compound::Quantum{.members = {}});
            const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, pose, std::move(actorQuantum), std::move(stateQuantum));
            return with<Block>::create(context, Block::Quantum{.body = body, .actor = actor});
        }

    } // namespace

    auto Block::Actions::spawnPlate(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, mech::plate::shape shape, mech::Role, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Id {
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::plate::perimeter.size()) {
            return context.refuse("eltanin::Block::spawnPlate: shape out of range");
        }
        return spawnWithLocals(context, root, pose, mech::plate::perimeter[index], std::move(actorQuantum), std::move(stateQuantum));
    }

    auto Block::Actions::spawnFrame(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, mech::frame::shape shape, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Id {
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::frame::corners.size()) {
            return context.refuse("eltanin::Block::spawnFrame: shape out of range");
        }
        return spawnWithLocals(context, root, pose, mech::frame::corners[index], std::move(actorQuantum), std::move(stateQuantum));
    }

    auto Block::Actions::spawnInner(Writing context, rmmr::scene::Root::Id root, rmmr::Pose pose, mech::frame::shape shape, mech::Role role, rmmr::scene::actor::Mesh::Quantum actorQuantum, rmmr::scene::actor::MeshState::Quantum stateQuantum) -> Id {
        stateQuantum.albedo = mech::settings::colorCode(role);
        const auto index = static_cast<std::size_t>(shape);
        if (index >= mech::frame::corners.size()) {
            return context.refuse("eltanin::Block::spawnInner: shape out of range");
        }
        return spawnWithLocals(context, root, pose, mech::frame::corners[index], std::move(actorQuantum), std::move(stateQuantum));
    }

    struct Block::Internals : Block::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, block] : context.proposal.aspect<Block>().items()) {
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, block.actor)) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, block.actor)->pose = body->pose();
            }
        }
    };

    auto Block::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Block, phys::Body, &Block::Quantum::body>{},
            reaction::structural::custody<Block, rmmr::scene::actor::Mesh, &Block::Quantum::actor>{},
            reaction::aspect_wide<Block, phys::Body>(&Block::Internals::followBody),
        };
    }
}
