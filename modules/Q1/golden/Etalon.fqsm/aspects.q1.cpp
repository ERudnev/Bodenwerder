#include <Etalon.fqsm/aspects.q1.h>

#include <fQSM/api/interface.h>

namespace Q1_fQSM::Etalon {
    using namespace fqsm::api;

    //
    // SampleEtity (reassembld)
    struct SampleEntity::Actions::Private : SampleEntity::Actions {
    };

    /*

    //
    // SampleEntity
    struct SampleEntity::Actions::Private : SampleEntity::Actions {

        // always: ?from_float(float) -> integer
        static auto from_float(float value_approximate) -> integer {
            const auto value = static_cast<integer>(value_approximate);
            return std::min(absolute_max, std::max(absolute_min, value));
        }

        // one: !min_value()
        static auto min_value(const Quantum& before) -> PossibleChange {
            if (before.data_field >= absolute_min)
                return std::nullopt;
            return Quantum{.data_field = absolute_min};
        }

        // one: !max_value()
        static auto max_value(const Quantum& before) -> PossibleChange {
            if (before.data_field <= absolute_max)
                return std::nullopt;
            return Quantum{.data_field = absolute_max};
        }

        // all: !some_logic_fieldwide_invariant()
        static void some_logic_fieldwide_invariant(Reacting context) {
            const auto& items = context.proposal.aspect<SampleEntity>().items();
            const auto cap = static_cast<std::size_t>(max_elements);
            if (items.size() <= cap)
                return;

            std::vector<Id> ids;
            ids.reserve(items.size());
            for (const auto entry : items)
                ids.push_back(entry.id);

            const std::size_t over = items.size() - cap;
            auto& patch = context.reaction<SampleEntity>();
            for (std::size_t i = 0; i < over; ++i)
                patch.put_deletion(ids[i]);
        }
    };

    struct some_logic_fieldwide_invariant final : reaction::Abstract {
        Sources listens() const override { return typed_set<SampleEntity>(); }
        void apply(Reacting context) override {
            SampleEntity::Actions::Private::some_logic_fieldwide_invariant(context);
        }
    };

    // all: >from_float(value_approximate: float) -> #
    auto SampleEntity::Actions::from_float(Writing context, float value_approximate) -> Id {
        return create(context, Quantum{.data_field = Private::from_float(value_approximate)});
    }

    // all: ?find_first(integer) -> #?
    auto SampleEntity::Actions::find_first(Reading context, integer sought) -> optional<Id> {
        for (const auto entry : context.aspect<SampleEntity>().items()) {
            if (entry.value.data_field == sought)
                return entry.id;
        }
        return std::nullopt;
    }

    // all: ?const_fieldwide_method() -> integer
    auto SampleEntity::Actions::const_fieldwide_method(Reading context) -> integer {
        integer sum = integer{0};
        for (const auto entry : context.aspect<SampleEntity>().items())
            sum += entry.value.data_field;
        return sum;
    }

    // all: =nonconst_fieldwide_method()
    void SampleEntity::Actions::nonconst_fieldwide_method(Writing context) {
        const auto& items = context->aspect<SampleEntity>().items();
        if (items.empty())
            return;

        auto it = items.begin();
        Id min_id = it->id;
        integer min_val = it->value.data_field;
        for (++it; it != items.end(); ++it) {
            const auto v = it->value.data_field;
            if (v < min_val) {
                min_val = v;
                min_id = it->id;
            }
        }
        remove(context, min_id);
    }

    // one: ?const_element_method() -> string
    auto SampleEntity::Actions::const_element_method(Reading context, Id id) -> string {
        return std::to_string(get(context, id).data_field);
    }

    // one: =nonconst_element_method()
    void SampleEntity::Actions::nonconst_element_method(Writing context, Id id) {
        modify(context, id)->data_field += integer{1};
    }

    const SampleEntity::Reactions::Behavior SampleEntity::Reactions::custom = {
        reaction::constraint::element<SampleEntity>(&Actions::Private::min_value),
        reaction::constraint::element<SampleEntity>(&Actions::Private::max_value),
        some_logic_fieldwide_invariant{},
    };
    */

}
