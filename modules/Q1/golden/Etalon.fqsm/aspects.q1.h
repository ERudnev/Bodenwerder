#pragma once

#include <fQSM/api/interface.h>

namespace Q1_fQSM::Etalon {

    using namespace fqsm::api;

    struct Trivia : Entity<Trivia> {
        struct Quantum {};
        //TODO: clarify to cleanup: using Actions = DefaultActions;
        using Reactions = DefaultReactions;
    };

    struct SampleEntity : Entity<SampleEntity> {
        static constexpr integer max_elements = integer{2000};
        static constexpr integer absolute_min = integer{-1000};
        static constexpr integer absolute_max = integer{1000};
        struct Quantum {
            integer data_field;
        };
        struct Global {
            integer common_data{};
        };
        struct Actions : BaseActions {
            struct Private;
            //@  methods from SampleEntity::one '?' '=' '>' dwelled here. NB: "reactions aka normalizaers"declared with '!' are ALVAYS private
            static auto const_element_method(Reading, Id) -> string;
            static void nonconst_element_method(Writing, Id);

            // @ methods from SampleEntity::all (all, except '!')
            static auto from_float(Writing, float value_approximate) -> Id;
            static auto find_first(Reading, integer) -> optional<Id>;
            static auto const_fieldwide_method(Reading) -> integer;
            static void nonconst_fieldwide_method(Writing);
        };
        struct Reactions : BaseReactions { static const Behavior custom; };
    };

    struct Tag : Attribute<Tag, SampleEntity> {
        struct Quantum{};
        struct Global {
            integer modulus = integer{2};
        };
        struct Reactions : BaseReactions { static const Behavior custom; };
    };

    struct Remnant : Component<Remnant, Tag> {
        struct Quantum {
            integer power;
            Trivia::Id trivia;
        };
        struct Reactions : BaseReactions { static const Behavior custom; };
    };

    struct SampleComponent : Component<SampleComponent, SampleEntity> {
        struct Quantum {};
        struct Actions : BaseActions{
            static void example_op_multiply(Writing, Id, integer factor);
            static auto example_op_div_with_remainder(Writing, Id, integer divisor)->integer; // returns remainder
        };
        struct Reactions : BaseReactions { static const Behavior custom; };
    };

    struct SampleAttribute : Attribute<SampleAttribute, SampleEntity> {
        struct Quantum {
            Anchor<Trivia> main_anchor;
            Control<Trivia> main_dummy;
        };
        struct Global {};
        struct Actions : BaseActions{
            static auto complex_constructor(Writing, SampleEntity::Id) -> Id;
        };
        struct Reactions : BaseReactions { static const Behavior custom; };
    };

    // experimental one-liner syntax for Q1 "one-liner entities"
    struct Note : Entity<Note> {
        struct Quantum { string text; };
        using Reactions = DefaultReactions;
    };

    struct Note_group : Group<Note_group, Note, SampleEntity> {};

    struct Notebook : Archetype<Notebook> {
        static auto notes_count(Reading, SampleEntity::Id)->integer;
        static auto add_note(Writing, decltype(Note::Quantum::text))-> Note::Id;
    };
}