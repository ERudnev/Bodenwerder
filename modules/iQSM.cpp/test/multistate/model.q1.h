#pragma once

#include <utility>

#include <iQSM/api/_gateway.h>

namespace RnD::Logic {

    using namespace iqsm::dsl_gateway;

    struct House : Entity<House>, Require<> {
        struct Quantum {
            integer happiness;
            integer dynamics;
        };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {};
    };

    struct Pairing : Entity<Pairing>, Require<House> {
        struct Quantum {
            std::pair<House::Id, House::Id> participants;
        };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {
            static void update(Writing, Id);
        };
    };
}

namespace RnD::View {

    using namespace iqsm::dsl_gateway;

    struct HappyHouse : Component<HappyHouse, Logic::House>, Require<Logic::House> {
        struct Quantum {
            integer previous_happiness;
        };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {
            static void update(Writing, Id);
        };
    };
}
