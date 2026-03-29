#pragma once

#include <iQSM/api/_gateway.h>

namespace Q1CORE::Etalon { 
    
    using namespace iqsm::dsl_gateway;

    struct SampleEntity : Entity<SampleEntity>, Require<> {
        struct Quantum {
            integer data_field;
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {
            static auto construct(Writing, integer data_field) -> Id;

            static auto example_const_fieldwide_method(Reading) -> string;
            static auto example_const_element_method(Reading, Id) -> float;
            static void example_nonconst_fieldwide_method(Writing);
            static void example_nonconst_element_method(Writing, Id);
        };
    };

    struct Tag : Attribute<Tag, SampleEntity>, Require<SampleEntity> {
        struct Quantum{};
        struct Global {
            integer modulus = integer{2};
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations{};
    };

    struct SampleComponent : Component<SampleComponent, SampleEntity>, Require<SampleEntity> {
        struct Quantum {
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations{};
    };

    struct SampleAttribute : Attribute<SampleAttribute, SampleEntity>, Require<SampleEntity, SampleComponent> {
        struct Quantum {
            Id neighbor_anchor;
            optional<SampleComponent::Id> optional_anchor;
            std::vector<SampleEntity::Id> every_essential;
            std::vector<SampleComponent::Id> at_least_one_required;
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations{};
    };
}