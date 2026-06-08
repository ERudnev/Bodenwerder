#pragma once

#include <fQSM/api/interface.h>

namespace Q1_iQSM::Etalon { 
    
    using namespace fqsm::api;

    struct Trivia : Entity<Trivia> {
        struct Quantum {};
        struct Global {};
        static const Codex codex;
        struct Service : BaseCapabilities{};
    };

    struct SampleEntity : Entity<SampleEntity> {
        struct Quantum {
            integer data_field;
        };
        struct Global {};
        static const Codex codex;
        struct Service : BaseCapabilities {
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

    struct Tag : Attribute<Tag, SampleEntity> {
        struct Quantum{};
        struct Global {
            integer modulus = integer{2};
        };
        static const Codex codex;
        struct Service : BaseCapabilities{};
    };

    struct Remnant : Component<Remnant, Tag> {
        struct Quantum {
            integer power;
            Trivia::Id trivia;
        };
        struct Global {};
        static const Codex codex;
        struct Service : BaseCapabilities{};
    };

    struct SampleComponent : Component<SampleComponent, SampleEntity> {
        struct Quantum {};
        struct Global {};
        static const Codex codex;
        struct Service : BaseCapabilities{
            static auto example_op_multiply(Writing, Id, integer factor)->void;
            static auto example_op_div_with_remainder(Writing, Id, integer divisor)->integer; // returns remainder
        };
    };

    struct SampleAttribute : Attribute<SampleAttribute, SampleEntity> {
        struct Quantum {
            Id neighbor_anchor;
            optional<SampleComponent::Id> optional_anchor;
            std::vector<SampleEntity::Id> every_essential;
            std::vector<SampleComponent::Id> at_least_one_required;
        };
        struct Global {};
        static const Codex codex;
        struct Manipulators : BaseCapabilities{
            static auto create_complex_constructor(Writing, SampleEntity::Id existing) -> Id;
        };
    };
}