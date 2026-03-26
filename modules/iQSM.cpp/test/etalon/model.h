#pragma once

#include <iQSM/api/_gateway.h>

namespace iqsm_internal_model { 
    
    using namespace iqsm::dsl_gateway;

    struct SampleEntity : Entity<SampleEntity>, Require<> {
        struct Quantum {
            integer data_field;
        };
        static const Invariants invariants;
        struct Operations : OwnTypeOperations {
            static auto const_fieldwide_method(Reading) -> string;
            static auto const_element_method(Reading, Id) -> float;

            static void nonconst_fieldwide_method(Writing);
            static void nonconst_element_method(Writing, Id);

            static auto public_constructor(Writing, integer data_field) -> Id;

        private:
            static auto public_constructor(Writing, Quantum) -> Id;
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