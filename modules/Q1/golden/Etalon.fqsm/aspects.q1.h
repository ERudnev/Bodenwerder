#pragma once

#include <fQSM/api/interface.h>

namespace Q1_iQSM::Etalon { 
    
    using namespace iqsm::q1_gateway;

    struct Trivia : Entity<Trivia>, Require<> {
        struct Quantum {};
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnManipulators{};
    };

    struct SampleEntity : Entity<SampleEntity>, Require<> {
        struct Quantum {
            integer data_field;
        };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnManipulators {
            static constexpr integer max_elements = integer{2000};
            static constexpr integer absolute_min = integer{-1000};
            static constexpr integer absolute_max = integer{1000};
            static auto from_float(float) -> integer;

            static auto create_from_float(Writing, float field_value) -> Id;

            static auto example_const_fieldwide_method(Reading) -> integer;
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
        struct Operations : OwnManipulators{};
    };

    struct Remnant : Component<Remnant, Tag>, Require<Tag, Trivia> {
        struct Quantum {
            integer power;
            Trivia::Id trivia;
        };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnManipulators{};
    };

    struct SampleComponent : Component<SampleComponent, SampleEntity>, Require<SampleEntity> {
        struct Quantum {};
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnManipulators{
            static auto example_op_multiply(Writing, Id, integer factor)->void;
            static auto example_op_div_with_remainder(Writing, Id, integer divisor)->integer; // returns remainder
        };
    };

    struct SampleAttribute : Attribute<SampleAttribute, SampleEntity>, Require<SampleEntity, SampleComponent> {
        struct Quantum {
            Id neighbor_anchor;
            optional<SampleComponent::Id> optional_anchor;
            std::vector<SampleEntity::Id> every_essential;
            std::vector<SampleComponent::Id> at_least_one_required;
        };
        struct Global {};
        static const Invariants invariants;
        struct Operations : OwnManipulators{
            static auto create_complex_constructor(Writing, SampleEntity::Id existing) -> Id;
        };
    };
}