#include <kubes/physics/atom.q1.h>

#include <vector>

namespace kubes::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    void Visual::Actions::update(Writing context) {
        std::vector<Id> doomed;
        for (const auto [id, visual] : context->aspect<Visual>().items()) {
            if (not my::relation(context, id, &Quantum::atom)
                or not my::ward(context, id, &Quantum::actor)) {
                doomed.push_back(id);
                continue;
            }
            const auto& atom = with<Atom>::get(context, visual.atom);
            with<scene::Node>::modify(context, visual.actor)->position = Pos{atom.current};
        }
        for (const auto id : doomed) {
            with<Visual>::remove(context, id);
        }
    }

    struct Visual::Internals : Visual::DefaultInternals {};

    auto Visual::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Visual, scene::Node, &Visual::Quantum::actor>{},
        };
    }

}
