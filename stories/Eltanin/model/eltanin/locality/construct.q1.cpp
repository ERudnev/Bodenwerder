#include <eltanin/locality/construct.q1.h>

#include "mech/assembler.h"
#include "mech/construction.h"
#include "physics/hullBvh.h"
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>

#include <span>

#include <glm/geometric.hpp>

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

        auto shedOne(Stewarding context, Construct::Id id, Construct::Quantum& construct) -> bool {
            if (not with<phys::rigid::Crystal>::exists(context, construct.body))
                return true;
            auto crystals = context.direct<phys::rigid::Crystal>();
            auto* crystal = crystals.items.find(construct.body);
            if (not crystal or crystal->particles.size() != construct.construction.evaluatedParticles.size() or crystal->particles.size() != crystal->shape.size())
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
            integer cursor = 0;
            bool mismatch = false;
            mech::forEachPrimitiveLoop(construct.construction, [&](mech::Construction::Primitive::Id primitiveId, const mech::Construction::Primitive& primitive) {
                if (mismatch)
                    return;
                const bool dead = dropPrimitive(worst, primitiveId);
                for (std::size_t slot = 0; slot < primitive.loop.size(); ++slot) {
                    const auto index = static_cast<std::size_t>(cursor);
                    if (index >= crystal->particles.size() or index >= crystal->shape.size()) {
                        mismatch = true;
                        return;
                    }
                    if (not dead) {
                        particles.push_back(crystal->particles[index]);
                        shape.push_back(crystal->shape[index]);
                    }
                    ++cursor;
                }
            });
            if (mismatch or static_cast<std::size_t>(cursor) != crystal->particles.size())
                return true;
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
            auto bodies = context.direct<phys::Body>();
            if (auto* body = bodies.items.find(construct.body))
                crystal->refreshMatter(*body);

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

    void Construct::Actions::update(Stewarding context) {
        shedDead(context);
    }

    void Construct::Actions::shedDead(Stewarding context) {
        vector<Id> gone;
        for (auto [id, construct] : context.direct<Construct>().items) {
            if (not shedOne(context, id, construct))
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
