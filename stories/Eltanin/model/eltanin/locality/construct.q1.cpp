#include <eltanin/locality/construct.q1.h>

#include "mech/construction.h"
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>

#include <span>

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

    }

    void Construct::Actions::update(Stewarding) {
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
