#include <eltanin/mech/construction.q1.h>

#include <rmmr/scene/node.q1.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Construct::Internals : Construct::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, construct] : context.proposal.aspect<Construct>().items()) {
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, construct.actor)) { my::remove(context, id); continue; }
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, construct.actor)->pose = body->pose();
            }
        }
    };

    auto Construct::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Construct, rmmr::scene::actor::Mesh, &Construct::Quantum::actor>{},
            reaction::aspect_wide<Construct, phys::Body>(&Construct::Internals::followBody),
        };
    }

}
