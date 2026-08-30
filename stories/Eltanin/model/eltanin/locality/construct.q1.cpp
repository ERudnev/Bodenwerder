#include <eltanin/locality/construct.q1.h>
#include <eltanin/locality/scrap.q1.h>

#include "mech/assembler.h"
#include "mech/construction.h"
#include "physics/hullBvh.h"
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>

#include <span>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>

namespace eltanin::locality {

    using namespace fqsm::api;

    namespace {

        auto cohesionByPrimitive(const mech::Construction& construction, const vector<phys::Particle>& particles) -> umap<mech::Construction::Primitive::Id, float> {
            umap<mech::Construction::Primitive::Id, float> worst;
            integer cursor = 0;
            mech::forEachPrimitiveLoop(construction, [&](mech::Construction::Primitive::Id id, const mech::Construction::Primitive& primitive) {
                const auto count = primitive.loop.size();
                for (std::size_t slot = 0; slot < count; ++slot) {
                    if (static_cast<std::size_t>(cursor) >= particles.size())
                        return;
                    const float cohesion = particles[static_cast<std::size_t>(cursor)].cohesion;
                    ++cursor;
                    auto found = worst.find(id);
                    if (found == worst.end())
                        worst.emplace(id, cohesion);
                    else if (cohesion < found->second)
                        found->second = cohesion;
                }
            });
            return worst;
        }

        auto dropPrimitive(const umap<mech::Construction::Primitive::Id, float>& worst, mech::Construction::Primitive::Id id) -> bool {
            const auto found = worst.find(id);
            return found != worst.end() and found->second <= 0.0f;
        }

        constexpr float minHalf = 0.25f;

        auto quatFromAxes(vec3 x, vec3 y, vec3 z) -> quat {
            return glm::normalize(glm::quat_cast(mat3{x, y, z}));
        }

        auto perpendicular(vec3 axis) -> vec3 {
            vec3 hint{0.0f, 1.0f, 0.0f};
            if (glm::abs(glm::dot(axis, hint)) > 0.9f)
                hint = vec3{1.0f, 0.0f, 0.0f};
            return glm::normalize(glm::cross(axis, hint));
        }

        struct LocalBox {
            vec3 center;
            quat rotation;
            vec3 half;
        };

        auto boxOf(const vector<vec3>& points, float thickness) -> LocalBox {
            const float shell = glm::max(thickness * 0.5f, 0.08f);
            const quat identity{1.0f, 0.0f, 0.0f, 0.0f};
            if (points.empty())
                return LocalBox{.center = vec3{0.0f, 0.0f, 0.0f}, .rotation = identity, .half = vec3{minHalf, minHalf, minHalf}};
            if (points.size() == 1)
                return LocalBox{.center = points[0], .rotation = identity, .half = vec3{shell, shell, shell}};
            if (points.size() == 2) {
                const vec3 delta = points[1] - points[0];
                const float length = glm::length(delta);
                if (length < 1.0e-5f)
                    return LocalBox{.center = points[0], .rotation = identity, .half = vec3{shell, shell, shell}};
                const vec3 x = delta / length;
                const vec3 y = perpendicular(x);
                const vec3 z = glm::cross(x, y);
                return LocalBox{.center = 0.5f * (points[0] + points[1]), .rotation = quatFromAxes(x, y, z), .half = vec3{length * 0.5f, shell, shell}};
            }
            vec3 sum{0.0f, 0.0f, 0.0f};
            for (const vec3& point : points)
                sum += point;
            const vec3 centroid = sum / static_cast<float>(points.size());
            vec3 normal = glm::cross(points[1] - points[0], points[2] - points[0]);
            const float normalLen = glm::length(normal);
            if (normalLen < 1.0e-8f)
                return LocalBox{.center = centroid, .rotation = identity, .half = vec3{shell, shell, shell}};
            normal /= normalLen;
            const vec3 x = glm::normalize(points[1] - points[0]);
            const vec3 y = glm::cross(normal, x);
            float minX = 1.0e30f;
            float maxX = -1.0e30f;
            float minY = 1.0e30f;
            float maxY = -1.0e30f;
            for (const vec3& point : points) {
                const vec3 delta = point - centroid;
                const float u = glm::dot(delta, x);
                const float v = glm::dot(delta, y);
                minX = glm::min(minX, u);
                maxX = glm::max(maxX, u);
                minY = glm::min(minY, v);
                maxY = glm::max(maxY, v);
            }
            const vec3 center = centroid + x * (0.5f * (minX + maxX)) + y * (0.5f * (minY + maxY));
            return LocalBox{.center = center, .rotation = quatFromAxes(x, y, normal), .half = vec3{glm::max(0.5f * (maxX - minX), minHalf), glm::max(0.5f * (maxY - minY), minHalf), shell}};
        }

        struct DeadChunk {
            float cohesion;
            float thickness;
            float mass;
            dvec3 momentum;
            vector<vec3> locals;
        };

        template<typename Piece, typename IdOf>
        void keepLive(vector<Piece>& items, IdOf idOf, const umap<mech::Construction::Primitive::Id, float>& worst) {
            vector<Piece> live;
            live.reserve(items.size());
            for (auto& piece : items) {
                if (not dropPrimitive(worst, idOf(piece)))
                    live.push_back(std::move(piece));
            }
            items = std::move(live);
        }

        auto shedOne(Writing context, Construct::Id id, Construct::Quantum& construct) -> bool {
            if (not with<phys::rigid::Crystal>::exists(context, construct.body))
                return true;
            auto crystal = with<phys::rigid::Crystal>::modify(context, construct.body);
            if (crystal->particles.size() != construct.construction.evaluatedParticles.size() or crystal->particles.size() != crystal->shape.size())
                return true;
            const auto worst = cohesionByPrimitive(construct.construction, crystal->particles);
            bool anyDead = false;
            for (const auto& [_, cohesion] : worst) {
                if (cohesion <= 0.0f) {
                    anyDead = true;
                    break;
                }
            }
            if (not anyDead)
                return true;

            vector<phys::Particle> particles;
            vector<vec3> shape;
            particles.reserve(crystal->particles.size());
            shape.reserve(crystal->shape.size());
            umap<mech::Construction::Primitive::Id, DeadChunk> dead;
            integer cursor = 0;
            bool mismatch = false;
            mech::forEachPrimitiveLoop(construct.construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (mismatch)
                    return;
                const bool gone = dropPrimitive(worst, primitiveId);
                DeadChunk* chunk = nullptr;
                if (gone) {
                    auto found = dead.find(primitiveId);
                    if (found == dead.end()) {
                        const auto cohesion = worst.find(primitiveId);
                        found = dead.emplace(primitiveId, DeadChunk{.cohesion = cohesion->second, .thickness = primitive.thickness, .mass = 0.0f, .momentum = dvec3{0.0, 0.0, 0.0}, .locals = {}}).first;
                    }
                    found->second.thickness = glm::max(found->second.thickness, primitive.thickness);
                    chunk = &found->second;
                }
                for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                    const auto index = static_cast<std::size_t>(cursor);
                    if (index >= crystal->particles.size() or index >= crystal->shape.size()) {
                        mismatch = true;
                        return;
                    }
                    if (gone) {
                        const auto& particle = crystal->particles[index];
                        chunk->locals.push_back(crystal->shape[index]);
                        chunk->mass += particle.mass;
                        chunk->momentum += particle.velocity() * double(particle.mass);
                    } else {
                        particles.push_back(crystal->particles[index]);
                        shape.push_back(crystal->shape[index]);
                    }
                    ++cursor;
                }
            });
            if (mismatch or static_cast<std::size_t>(cursor) != crystal->particles.size())
                return true;

            if (with<Thing>::get_global(context).scene) {
                if (with<phys::Body>::exists(context, construct.body)) {
                    const auto& body = with<phys::Body>::get(context, construct.body);
                    for (const auto& [_, chunk] : dead) {
                        if (chunk.mass <= 0.0f or chunk.locals.empty())
                            continue;
                        const auto box = boxOf(chunk.locals, chunk.thickness);
                        const vec3 worldCenter = vec3{body.position} + body.orientation * box.center;
                        const quat worldRot = glm::normalize(body.orientation * box.rotation);
                        const vec3 linear = vec3{chunk.momentum / double(chunk.mass)};
                        Scrap::Actions::breakOff(context, worldCenter, worldRot, box.half, chunk.mass, linear, chunk.cohesion);
                    }
                }
            }

            if (particles.empty())
                return false;

            glm::dvec3 moment{0.0, 0.0, 0.0};
            double mass = 0.0;
            for (std::size_t index = 0; index < particles.size(); ++index) {
                moment += glm::dvec3{shape[index]} * static_cast<double>(particles[index].mass);
                mass += static_cast<double>(particles[index].mass);
            }
            auto eraseDead = [&](auto& items) {
                for (auto it = items.begin(); it != items.end(); ) {
                    if (dropPrimitive(worst, it->first))
                        it = items.erase(it);
                    else
                        ++it;
                }
            };
            eraseDead(construct.construction.knots);
            eraseDead(construct.construction.ribs);
            eraseDead(construct.construction.membranes);
            eraseDead(construct.construction.plates);
            eraseDead(construct.construction.volumes);
            keepLive(construct.fragments.ofKnot, [](const auto& piece) { return piece.knot; }, worst);
            keepLive(construct.fragments.ofRib, [](const auto& piece) { return piece.rib; }, worst);
            keepLive(construct.fragments.ofMembrane, [](const auto& piece) { return piece.membrane; }, worst);
            keepLive(construct.fragments.ofPlate, [](const auto& piece) { return piece.plate; }, worst);
            keepLive(construct.fragments.ofVolume, [](const auto& piece) { return piece.volume; }, worst);
            mech::compileParticles(construct.construction);
            crystal->particles = std::move(particles);
            crystal->shape = std::move(shape);
            crystal->com = mass > 0.0 ? vec3{moment / mass} : vec3{0.0f, 0.0f, 0.0f};
            crystal->hull = mech::cookHull(construct.construction, crystal->shape);
            phys::collision::cookHullBvh(crystal->hull, crystal->shape);
            if (with<phys::Body>::exists(context, construct.body))
                crystal->refreshMatter(*with<phys::Body>::modify(context, construct.body));

            const auto interframe = with<rmmr::resource::Assets>::find<rmmr::resource::meshpack::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "interframe"));
            if (interframe and with<rmmr::scene::actor::Mesh>::exists(context, construct.actor)) {
                auto occurrences = mech::cookOccurrences(context, *interframe, construct.construction, construct.fragments, construct.visualOf);
                if (not occurrences.empty()) {
                    auto meshQuantum = with<rmmr::scene::actor::Mesh>::compose(context, occurrences);
                    if (meshQuantum)
                        with<rmmr::scene::actor::Mesh>::replace(context, construct.actor, std::move(*meshQuantum));
                }
            }
            Construct::Actions::syncVisualCohesion(context, id);
            return true;
        }

    }

    void Construct::Actions::update(Writing context) {
        shedDead(context);
    }

    void Construct::Actions::shedDead(Writing context) {
        vector<Id> gone;
        vector<Id> living;
        for (auto [id, _] : context->aspect<Construct>().items())
            living.push_back(id);
        for (const auto id : living) {
            auto construct = with<Construct>::modify(context, id);
            if (not shedOne(context, id, *construct))
                gone.push_back(id);
        }
        for (const auto id : gone)
            with<Construct>::kraken(context, id);
    }

    struct Construct::Internals : Construct::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, construct] : context.proposal.aspect<Construct>().items()) {
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, construct.actor)) { my::remove(context, id); continue; }
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, construct.actor)->pose = body->pose();
                Construct::Actions::syncVisualCohesion(context, id);
            }
        }
    };

    void Construct::Actions::syncVisualCohesion(Reading context, Id id) {
        const auto& construct = with<Construct>::get(context, id);
        if (not with<rmmr::scene::actor::Mesh>::exists(context, construct.actor)) return;
        if (not with<phys::rigid::Crystal>::exists(context, construct.body)) return;
        const auto& crystal = with<phys::rigid::Crystal>::get(context, construct.body);
        if (crystal.particles.size() != construct.construction.evaluatedParticles.size()) return;
        const auto worst = cohesionByPrimitive(construct.construction, crystal.particles);
        vector<float> cohesions;
        cohesions.reserve(construct.visualOf.size());
        for (const auto primitive : construct.visualOf) {
            const auto found = worst.find(primitive);
            cohesions.push_back(found == worst.end() ? 1.0f : found->second);
        }
        with<rmmr::scene::actor::Mesh>::writeCohesions(context, construct.actor, std::span<const float>{cohesions});
    }

    auto Construct::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Construct, rmmr::scene::actor::Mesh, &Construct::Quantum::actor>{},
            reaction::structural::custody<Construct, phys::rigid::Crystal, &Construct::Quantum::body>{},
            reaction::aspect_wide<Construct, phys::Body>(&Construct::Internals::followBody),
        };
    }

}
