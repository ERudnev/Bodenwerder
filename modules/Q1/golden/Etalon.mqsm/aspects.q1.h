#pragma once

#include <mQSM/api/_gateway.h>

namespace Q1_mQSM::Etalon {
    using namespace mqsm::q1_gateway;

    struct Trivia {
        struct Quantum{};
        struct Global{};
    };

    struct SampleEntity {
        struct Quantum {
            integer data_field;
        };
        struct Global{};
    };

    struct Tag {
        struct Quantum{};
        struct Global {
            integer modulus = integer{2};
        };
    };

    struct Remnant {
        struct Quantum {
            integer power;
            Trivia::Id trivia;
        };
        struct Global{};
    };

    struct SampleComponent {
        struct Quantum{};
        struct Global{};
    };

    struct SampleAttribute {
        struct Quantum {
            Id neighbor_anchor;
            optional<SampleComponent::Id> optional_anchor;
            std::vector<SampleEntity::Id> every_essential;
            std::vector<SampleComponent::Id> at_least_one_required;
        };
        struct Global{};
    };

}

