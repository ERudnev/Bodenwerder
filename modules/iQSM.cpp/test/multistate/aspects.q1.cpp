#include "aspects.q1.h"

#include <iQSM/api/_gateway_equal.h>

namespace RnD::Logic {

    using namespace iqsm::dsl_gateway;

    const Invariants House::invariants{
        .structural = {},
        .logical = {},
    };

    void Pairing::Operations::update(Writing commit, Id pairing_id) {
        const auto world = commit.initial;
        const auto& pr = ops::particle::get<Pairing>(world, pairing_id).participants;
        if (pr.first == pr.second) return;

        const auto ha = ops::particle::get<House>(world, pr.first).happiness;
        const auto hb = ops::particle::get<House>(world, pr.second).happiness;

        if (ha < hb) {
            ops::particle::modifier<House>(commit, pr.first)->happiness = ha + integer{1};
            ops::particle::modifier<House>(commit, pr.second)->happiness = hb - integer{1};
        } else if (ha > hb) {
            ops::particle::modifier<House>(commit, pr.first)->happiness = ha - integer{1};
            ops::particle::modifier<House>(commit, pr.second)->happiness = hb + integer{1};
        }
    }

    const Invariants Pairing::invariants{
        .structural = {
            invariant::anchor_all<House, Pairing, &Pairing::Quantum::participants>,
        },
        .logical = {},
    };
}

namespace RnD::View {

    using namespace iqsm::dsl_gateway;

    struct HappyHouse_private : HappyHouse::Operations {
        static auto construct(Writing commit, Logic::House::Id id, Item<Logic::House>)
            -> HappyHouse::Quantum {
            return HappyHouse::Quantum{
                .previous_happiness = ops::particle::get<Logic::House>(commit.initial, id).happiness,
            };
        }
    };

    void HappyHouse::Operations::update(Writing commit, Id id) {
        const auto current = ops::particle::get<Logic::House>(commit.initial, id).happiness;
        ops::particle::modifier<HappyHouse>(commit, id)->previous_happiness = current;
    }

    const Invariants HappyHouse::invariants{
        .structural = {
            invariant::isomorphic<Logic::House, HappyHouse, &HappyHouse_private::construct>,
        },
        .logical = {},
    };
}
