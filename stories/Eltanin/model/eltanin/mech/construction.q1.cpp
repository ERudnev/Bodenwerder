#include <eltanin/mech/construction.q1.h>

#include <eltanin/physics/rigid.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>

#include <algorithm>
#include <span>

namespace eltanin::mech {

    using namespace fqsm::api;

    namespace {

        auto loopCohesion(const Construction::Primitive& primitive, const vector<phys::Particle>& particles) -> float {
            float worst = 1.0f;
            bool any = false;
            for (const auto& welded : primitive.loop) {
                if (welded.particle < 0 or static_cast<std::size_t>(welded.particle) >= particles.size())
                    continue;
                worst = std::min(worst, particles[static_cast<std::size_t>(welded.particle)].cohesion);
                any = true;
            }
            return any ? worst : 1.0f;
        }

        auto primitiveCohesion(const Construction& construction, Construction::Primitive::Id id, const vector<phys::Particle>& particles) -> float {
            if (const auto knot = construction.knots.find(id); knot != construction.knots.end())
                return loopCohesion(knot->second, particles);
            if (const auto rib = construction.ribs.find(id); rib != construction.ribs.end())
                return loopCohesion(rib->second, particles);
            if (const auto membrane = construction.membranes.find(id); membrane != construction.membranes.end())
                return loopCohesion(membrane->second, particles);
            if (const auto plate = construction.plates.find(id); plate != construction.plates.end())
                return loopCohesion(plate->second, particles);
            if (const auto volume = construction.volumes.find(id); volume != construction.volumes.end()) {
                float worst = 1.0f;
                bool any = false;
                for (const auto& face : volume->second) {
                    worst = std::min(worst, loopCohesion(face, particles));
                    any = true;
                }
                return any ? worst : 1.0f;
            }
            return 1.0f;
        }

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
        vector<float> cohesions;
        cohesions.reserve(construct.visualOf.size());
        for (const auto primitive : construct.visualOf)
            cohesions.push_back(primitiveCohesion(construct.construction, primitive, crystal.particles));
        with<rmmr::scene::actor::Mesh>::writeCohesions(context, construct.actor, std::span<const float>{cohesions});
    }

    auto Construct::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Construct, rmmr::scene::actor::Mesh, &Construct::Quantum::actor>{},
            reaction::aspect_wide<Construct, phys::Body>(&Construct::Internals::followBody),
        };
    }

}
