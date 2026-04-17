#include "model.q1.h"

#include <algorithm>

#include <iQSM/api/_gateway_equal.h>

namespace RnD::Logic {

    using namespace iqsm::dsl_gateway;

    struct House_private : House::Operations {
        static auto happiness_clamped(Reading, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.happiness = std::clamp(after.happiness, integer{0}, integer{20});

            if (ops::particle::equal<House>(after, before)) return {};
            return after;
        }
    };

    const Invariants House::invariants{
        .structural = {},
        .logical = {
            invariant::for_each_item<House, &House_private::happiness_clamped>,
        },
    };

    void Pairing::Operations::update(Writing commit, Id pairing_id) {
        repo::Accumulator acc{commit};
        const auto& pr = ops::particle::get<Pairing>(acc, pairing_id).participants;
        if (pr.first == pr.second) return;

        const auto ha = ops::particle::get<House>(acc, pr.first).happiness;
        const auto hb = ops::particle::get<House>(acc, pr.second).happiness;

        if (ha < hb) {
            ops::particle::modifier<House>(acc, pr.first)->happiness = ha + integer{2};
            ops::particle::modifier<House>(acc, pr.second)->happiness = hb + integer{1};
        } else if (ha > hb) {
            ops::particle::modifier<House>(acc, pr.first)->happiness = ha + integer{1};
            ops::particle::modifier<House>(acc, pr.second)->happiness = hb + integer{2};
        } else {
            ops::particle::modifier<House>(acc, pr.first)->happiness = ha + integer{1};
            ops::particle::modifier<House>(acc, pr.second)->happiness = hb + integer{1};
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
        static auto construct(Reading context, Logic::House::Id id, Item<Logic::House>) -> HappyHouse::Quantum {
            return HappyHouse::Quantum{
                .previous_happiness = ops::particle::get<Logic::House>(context, id).happiness,
            };
        }
    };

    void HappyHouse::Operations::update(Writing commit, Id id) {
        repo::Accumulator acc{commit};
        const auto current = ops::particle::get<Logic::House>(acc, id).happiness;
        const auto previous = ops::particle::get<HappyHouse>(acc, id).previous_happiness;
        ops::particle::modifier<Logic::House>(acc, id)->dynamics = current - previous;
        ops::particle::modifier<HappyHouse>(acc, id)->previous_happiness = current;
    }

    const Invariants HappyHouse::invariants{
        .structural = {
            invariant::isomorphic<Logic::House, HappyHouse, &HappyHouse_private::construct>,
        },
        .logical = {},
    };
}
