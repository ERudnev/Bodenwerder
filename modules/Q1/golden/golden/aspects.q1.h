#pragma once

#include <fQSM/api/interface.h>

namespace Q1_fQSM::Etalon {

    using namespace fqsm::api;

    //@ in is not forward, it is mature Aspect definition (just minimal Aspect)
    struct Trivia : Entity<Trivia> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Origin : Entity<Origin> {
        struct Quantum {};
        struct Global {
            Trivia::Id trivia;
        };
        struct Always {
            static auto assemble(SettingUp&) -> Global;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct SampleEntity : Entity<SampleEntity> {
        //@ always: treat constants as static constexpr in Always; pure functions likewise
        struct Always {
            static constexpr integer max_elements = 2000;
            static constexpr integer absolute_min = -1000;
            static constexpr integer absolute_max = 1000;
            //@ always-level functions have no additional parameters
            static auto from_float(float value_approximate) -> integer;
        };
        //@ one: item-level data
        struct Quantum {
            integer data_field;
        };
        //@ all: field-own data (one Global per aspect in world instance)
        struct Global {
            integer common_data{};
        };
        struct Actions : BaseActions {
            //@ methods from SampleEntity::one '?' '=' '>' dwell here
            //@ NB: "reactions aka normalizaers"declared with '!' are ALVAYS hidden in Internals
            static auto const_element_method(Reading, Id) -> string;
            static void nonconst_element_method(Writing, Id);

            //@ methods from SampleEntity::all (all, except '!')
            static auto from_float(Writing, float value_approximate) -> Id;
            static auto find_first(Reading, integer) -> optional<Id>;
            static auto const_fieldwide_method(Reading) -> integer;
            static void nonconst_fieldwide_method(Writing);
        };
        //@ '!': Internals + customAspectReactions — one: min/max; all: set cardinality invariant
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Tag : Attribute<Tag, SampleEntity> {
        struct Quantum {};
        struct Global {
            integer modulus = 2;
        };
        //@ all-reaction !modulus_clamped(~) — set-as-set (Global)
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Clock : Entity<Clock> {
        struct Quantum {
            timepoint current{};
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    //@ all *name(~Scope, args...) — Stewarding / Direct hot pass (not Writing).
    //@ Scope `~` / `~Type` declares which aspects are opened via direct<>; it is not a C++ parameter.
    struct SpeedUpdate : Attribute<SpeedUpdate, SampleEntity> {
        struct Always {
            static constexpr integer max_speed = 3;
        };
        struct Quantum {
            integer speed; // increase of parent data_field per tick
        };
        struct Actions : BaseActions {
            //@ *active_maintenance_call(~) — fast linear pass to clamp own values
            static void active_maintenance_call(Stewarding);
            //@ *active_working_call(~SampleEntity, tick_count: integer) — apply speed to parent data_field
            static void active_working_call(Stewarding, integer tick_count);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct ReactionSketch : Entity<ReactionSketch> {
        struct Quantum {
            string value;
        };
        struct Actions : BaseActions {
            static void field_action(Writing);
        };
        //@ one-reactions: normalize(=one); watches (~ / ~Clock / ~SampleEntity) — Internals
        struct Internals;
        static const Behavior customAspectReactions();
    };

    //@ affects<>: typed Id link (no structural lifecycle); sibling of anchor<> / custody<>
    struct Reminder : Entity<Reminder> {
        struct Quantum {
            Affected<SampleEntity> target;
            integer trigger_value;
        };
        struct Actions : BaseActions {
            static auto add_to(Writing, SampleEntity::Id, integer value) -> Id;
        };
        //@ one-reactions !remove_after_happened(~) / !write_log_when_reached(~SampleEntity)
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Remnant : Component<Remnant, Tag> {
        struct Quantum {
            integer power;
        };
        //@ one-reaction !sync(~Tag)
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct SampleComponent : Component<SampleComponent, SampleEntity> {
        struct Quantum {};
        struct Actions : BaseActions {
            static void example_op_multiply(Writing, Id, integer factor);
            static auto example_op_div_with_remainder(Writing, Id, integer divisor) -> integer;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct SampleAttribute : Attribute<SampleAttribute, SampleEntity> {
        struct Quantum {
            Anchor<Trivia> main_anchor;
            Custody<Trivia> main_dummy;
        };
        struct Actions : BaseActions {
            static auto create(Writing, integer sample_value) -> Id;
            static void extend(Writing, SampleEntity::Id);
        };
        //@ anchor/custody + all-reaction !limit_by_tag_count(~Tag) — set-as-set
        struct Internals;
        static const Behavior customAspectReactions();
    };

    //@ experimental one-line form of syntax (sorry, parser!)
    struct Note : Entity<Note> {
        struct Quantum { string text; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Note_group : Group<Note_group, SampleEntity, Note> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    //@ Hint: '=' functions aka "item modifiers" are meaningless for Archetype (has no own state/quantum)
    struct Notebook : Archetype<Notebook> {
        static auto notes_count(Reading, SampleEntity::Id) -> integer;
        static auto add_note(Writing, SampleEntity::Id, decltype(Note::Quantum::text)) -> Note::Id;
    };
}

