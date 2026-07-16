#include <Etalon.fqsm/aspects.q1.h>

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace Q1_fQSM::Etalon {

    using namespace fqsm::api;

    struct SampleEntity::Internals : SampleEntity::DefaultInternals {
        static auto min_value(const Quantum& inspected) -> PossibleChange {
            if (inspected.data_field >= Always::absolute_min)
                return {};
            else
                return PossibleChange{Always::absolute_min};
        }

        static auto max_value(const Quantum& inspected) -> PossibleChange {
            if (inspected.data_field <= Always::absolute_max)
                return {};
            else
                return PossibleChange{Always::absolute_max};
        }

        static void some_logic_fieldwide_invariant(Reacting context) {
            const auto& items = context.proposal.aspect<SampleEntity>().items();
            const auto cap = static_cast<std::size_t>(Always::max_elements);
            if (items.size() <= cap)
                return;

            std::vector<Id> victims;
            victims.reserve(items.size() - cap);

            std::size_t index = 0;
            for (const auto entry : items) {
                if (index++ >= cap)
                    victims.push_back(entry.id);
            }

            auto& patch = context.reaction<SampleEntity>();
            for (const auto id : victims)
                patch.put_deletion(id);
        }
    };

    auto SampleEntity::Always::from_float(float value_approximate) -> integer {
        const auto value = static_cast<integer>(value_approximate);
        return std::min(absolute_max, std::max(absolute_min, value));
    }

    auto SampleEntity::Actions::const_element_method(Reading context, Id id) -> string {
        return std::to_string(get(context, id).data_field);
    }

    void SampleEntity::Actions::nonconst_element_method(Writing context, Id id) {
        modify(context, id)->data_field += integer{1};
    }

    auto SampleEntity::Actions::from_float(Writing context, float value_approximate) -> Id {
        const auto id = create(context, Quantum{.data_field = Always::from_float(value_approximate)});
        with<SampleComponent>::extend(context, id, {});
        return id;
    }

    auto SampleEntity::Actions::find_first(Reading context, integer sought) -> optional<Id> {
        for (const auto entry : context->aspect<SampleEntity>().items()) {
            if (entry.value.data_field == sought)
                return entry.id;
        }
        return std::nullopt;
    }

    auto SampleEntity::Actions::const_fieldwide_method(Reading context) -> integer {
        integer sum = integer{0};
        for (const auto entry : context->aspect<SampleEntity>().items())
            sum += entry.value.data_field;
        return sum;
    }

    void SampleEntity::Actions::nonconst_fieldwide_method(Writing context) {
        const auto& items = context->aspect<SampleEntity>().items();
        if (items.empty())
            return;

        auto it = items.begin();
        Id min_id = it->id;
        integer min_value = it->value.data_field;

        for (++it; it != items.end(); ++it) {
            const auto candidate = it->value.data_field;
            if (candidate < min_value) {
                min_value = candidate;
                min_id = it->id;
            }
        }

        remove(context, min_id);
    }

    auto SampleEntity::customAspectReactions() -> const Behavior {
        return {
            reaction::constraint::element<SampleEntity>(&SampleEntity::Internals::min_value),
            reaction::constraint::element<SampleEntity>(&SampleEntity::Internals::max_value),
            reaction::aspect_wide<SampleEntity>(&SampleEntity::Internals::some_logic_fieldwide_invariant),
        };
    }

    struct Tag::Internals : Tag::DefaultInternals {
        static void modulus_clamped(Reacting context) {
            auto fixed = context.proposal.aspect<Tag>().global();
            if (fixed.modulus > integer{0})
                return;

            fixed.modulus = integer{1};
            context.reaction<Tag>().put_global(fixed);
        }
    };

    auto Tag::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Tag>(&Tag::Internals::modulus_clamped),
        };
    }

    struct Remnant::Internals : Remnant::DefaultInternals {
        static auto evaluate_sync(Reading context, Id id, const Quantum& last_value) -> PossibleChange {
            const auto modulus = with<Tag>::get_global(context).modulus;
            if (modulus <= integer{0})
                return {};

            const auto expected_power = with<SampleEntity>::get(context, id).data_field / modulus;
            if (last_value.power == expected_power)
                return {};

            return PossibleChange{expected_power};
        }

        static void sync(Reacting context) {
            auto& patch = context.reaction<Remnant>();

            // wave 1: Tag updates may force recomputation of matching Remnant items
            for (const auto change : context.changes<Tag>().addedOrUpdated()) {
                const auto fix = evaluate_sync(context, change.id, with<Remnant>::get(context, change.id));
                if (fix) patch.put_modification(change.id, *fix);
            }

            // wave 2: direct Remnant updates use the same evaluator
            for (const auto change : context.changes<Remnant>().addedOrUpdated()) {
                const auto fix = evaluate_sync(context, change.id, *change.after);
                if (fix) patch.put_modification(change.id, *fix);
            }
        }
    };

    auto Remnant::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Remnant, Tag>(&Remnant::Internals::sync),
        };
    }

    void SampleComponent::Actions::example_op_multiply(Writing context, Id id, integer factor) {
        with<SampleEntity>::modify(context, id)->data_field *= factor;
    }

    auto SampleComponent::Actions::example_op_div_with_remainder(Writing context, Id id, integer divisor) -> integer {
        const auto safe_divisor = divisor > integer{0} ? divisor : integer{1};
        auto target = with<SampleEntity>::modify(context, id);
        const auto remainder = target->data_field % safe_divisor;
        target->data_field /= safe_divisor;
        return remainder;
    }

    struct SampleAttribute::Internals : SampleAttribute::DefaultInternals {
        static void limit_by_tag_count(Reacting context) {
            const auto attributes_count = context.proposal.aspect<SampleAttribute>().items().size();
            const auto tags_count = context.proposal.aspect<Tag>().items().size();
            if (attributes_count <= tags_count)
                return;

            context.deny(std::format("SampleAttribute count ({}) exceeds Tag count ({})", attributes_count, tags_count));
        }
    };

    auto SampleAttribute::Actions::create(Writing context, integer sample_value) -> Id {
        const auto created = with<SampleEntity>::create(context, SampleEntity::Quantum{sample_value});
        with<SampleComponent>::extend(context, created, {});

        const auto trivia = with<Trivia>::create(context, {});
        BaseActions::extend(context, created, SampleAttribute::Quantum{
            .main_anchor = trivia,
            .main_dummy = trivia,
        });
        return created;
    }

    void SampleAttribute::Actions::extend(Writing context, SampleEntity::Id parent) {
        with<SampleComponent>::extend(context, parent, {});
        const auto trivia = with<Trivia>::create(context, {});
        BaseActions::extend(context, parent, SampleAttribute::Quantum{
            .main_anchor = trivia,
            .main_dummy = trivia,
        });
    }

    auto SampleAttribute::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::anchored<SampleAttribute, Trivia, &SampleAttribute::Quantum::main_anchor>{},
            reaction::structural::controls<SampleAttribute, Trivia, &SampleAttribute::Quantum::main_dummy>{},
            reaction::aspect_wide<SampleAttribute, Tag>(&SampleAttribute::Internals::limit_by_tag_count),
        };
    }

    auto Notebook::notes_count(Reading context, SampleEntity::Id id) -> integer {
        if (not with<Note_group>::exists(context, id))
            return integer{0};

        return static_cast<integer>(with<Note_group>::get(context, id).size());
    }

    auto Notebook::add_note(Writing context, SampleEntity::Id id, decltype(Note::Quantum::text) text) -> Note::Id {
        if (not with<Note_group>::exists(context, id))
            with<Note_group>::extend(context, id);

        return with<Note_group>::addElement(context, id, Note::Quantum{
            .text = std::move(text),
        });
    }
}