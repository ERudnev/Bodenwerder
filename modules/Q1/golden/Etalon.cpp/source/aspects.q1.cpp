#include <Etalon/aspects.q1.h>

#include <iQSM/api/_gateway_equal.h>

#include <algorithm>

namespace Q1CORE::Etalon {

    using namespace iqsm::dsl_gateway;

    //
    // Trivia
    const Invariants Trivia::invariants{
        .structural = {},
        .logical = {},
    };

    //
    // SampleEntity
    struct SampleEntity_private : SampleEntity::Operations {

        static auto example_private_const_fieldwide_method(Reading) -> string { return {}; }
        static auto example_private_const_element_method(Reading, Id) -> float { return 0.0f; }

        static void example_private_nonconst_fieldwide_method(Writing) {}
        static void example_private_nonconst_element_method(Writing, Id) {}

        static auto min_value(Reading, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.data_field = std::max(after.data_field, integer{-1000});

            if (ops::particle::equal<SampleEntity>(after, before)) return {};
            return after;
        }

        static auto max_value(Reading, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.data_field = std::min(after.data_field, integer{1000});

            if (ops::particle::equal<SampleEntity>(after, before)) return {};
            return after;
        }
        static void example_private_validate_table(Writing) {}
    };
    auto SampleEntity::Operations::create_from_float(Writing gate, float field_value) -> Id {
        return ops::particle::create<SampleEntity>(gate, Quantum{static_cast<integer>(field_value)});
    }
    auto SampleEntity::Operations::example_const_fieldwide_method(Reading) -> string { return {}; }
    auto SampleEntity::Operations::example_const_element_method(Reading, Id) -> float { return 0.0f; }
    void SampleEntity::Operations::example_nonconst_fieldwide_method(Writing) {}
    void SampleEntity::Operations::example_nonconst_element_method(Writing, Id) {}    

    const Invariants SampleEntity::invariants{
        .structural = {},
        .logical = {
            invariant::for_each_item<SampleEntity, &SampleEntity_private::min_value>,
            invariant::for_each_item<SampleEntity, &SampleEntity_private::max_value>,
            &SampleEntity_private::example_private_validate_table,
        },
    };

    //
    // Tag (exists for even SampleEntity::data_field)
    struct Tag_private : Tag::Operations {
        static void modulus_clamped(Writing commit) {
            auto g = ops::global::modifier<Tag>(commit);
            if (g->modulus > integer{0}) return;
            g->modulus = integer{1};
        }

        static bool needs_tag(Reading world, SampleEntity::Id, Item<SampleEntity> item) {
            const auto mod = ops::global::get<Tag>(world)->modulus;
            return (item->data_field % mod) == integer{0};
        }
    };

    const Invariants Tag::invariants{
        .structural = {
            invariant::anchor_attribute<SampleEntity, Tag>,
        },
        .logical = {
            &Tag_private::modulus_clamped,
            invariant::existence<Tag, SampleEntity, &Tag_private::needs_tag>,
        },
    };

    //
    // Remnant
    struct Remnant_private : Remnant::Operations {
        static auto construct(Reading world, Id id, Item<Tag>) -> Quantum {
            return Quantum{
                .power = ops::particle::get<SampleEntity>(world, id).data_field / ops::global::get<Tag>(world)->modulus,
                .trivia = Trivia::Id{ id.raw() },
            };
        }

        static auto renmant_calculated(Reading world, Id id, const Quantum& before) -> ItemChange {
            const auto expected_power = ops::particle::get<SampleEntity>(world, id).data_field / ops::global::get<Tag>(world)->modulus;
            auto after = before;
            after.power = expected_power;
            if (ops::particle::equal<Remnant>(after, before)) return {};
            return after;
        }
    };

    void Remnant::Operations::pre_remove_action(Writing commit, Id, const Quantum& before) {
        //if (!ops::particle::exists<Trivia>(commit.initial, before.trivia)) return;
        //ops::particle::remove<Trivia>(commit, before.trivia);
    }

    const Invariants Remnant::invariants{
        .structural = {
            invariant::isomorphic<Tag, Remnant, &Remnant_private::construct>,
        },
        .logical = {
            invariant::for_each_item<Remnant, &Remnant_private::renmant_calculated>,
        },
    };

    //
    // SampleComponent
    struct SampleComponent_private : SampleComponent::Operations {
        static auto private_construct(Reading, Id, Item<SampleEntity>) -> Quantum { return {}; }
    };

    void SampleComponent::Operations::example_op_multiply(Writing commit, Id id, integer factor) {
        ops::particle::modifier<SampleEntity>(commit, id)->data_field *= factor;
    }

    auto SampleComponent::Operations::example_op_div_with_remainder(Writing commit, Id id, integer divisor) -> integer {
        const auto safe = divisor > integer{0} ? divisor : integer{1};
        auto m = ops::particle::modifier<SampleEntity>(commit, id);
        const auto remainder = (m->data_field % safe);
        m->data_field /= safe;
        return remainder;
    }
    
    const Invariants SampleComponent::invariants{
        .structural = {
            invariant::isomorphic<SampleEntity, SampleComponent, &SampleComponent_private::private_construct>,
        },
        .logical = {},
    };

    //
    // SampleAttribute
    struct SampleAttribute_private : SampleAttribute::Operations {
        static bool need_even(Reading, SampleEntity::Id, Item<SampleEntity> item) {
            return (item->data_field % integer{2}) == integer{0};
        }

        static auto construct(Reading, Id id, Item<SampleEntity>) -> Quantum {
            return Quantum{
                .neighbor_anchor = id,
                .optional_anchor = id,
                .every_essential = { id },
                .at_least_one_required = { id },
            };
        }
    };

    const Invariants SampleAttribute::invariants{
        .structural = {
            invariant::anchor_attribute<SampleEntity, SampleAttribute>,
            invariant::anchor<SampleEntity, SampleAttribute, &SampleAttribute::Quantum::neighbor_anchor>,
            invariant::anchor_optional<SampleComponent, SampleAttribute, &SampleAttribute::Quantum::optional_anchor>,
            invariant::anchor_all<SampleEntity, SampleAttribute, &SampleAttribute::Quantum::every_essential>,
            invariant::anchor_any<SampleEntity, SampleAttribute, &SampleAttribute::Quantum::at_least_one_required>,
        },
        .logical = {
            invariant::existence<SampleAttribute, SampleEntity, &SampleAttribute_private::need_even, &SampleAttribute_private::construct>,
        },
    };
}