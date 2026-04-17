#include <Etalon/aspects.q1.h>

#include <iQSM/api/_gateway_equal.h>

#include <algorithm>
#include <vector>

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

        //@ Reading interface is required for possible other aspects
        static auto min_value(Reading, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.data_field = std::max(after.data_field, absolute_min);

            if (ops::particle::equal<SampleEntity>(after, before)) return {};
            return after;
        }

        static auto max_value(Reading, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.data_field = std::min(after.data_field, absolute_max);

            if (ops::particle::equal<SampleEntity>(after, before)) return {};
            return after;
        }

        static void some_logic_fieldwide_invariant(Writing commit) {
            repo::Sequence tx{commit};
            const auto& container = tx->field<SampleEntity>()->container;
            const auto cap = static_cast<std::size_t>(max_elements);
            if (container.size() <= cap) return;

            std::vector<Id> ids;
            ids.reserve(container.size());
            for (const auto& kv : container) {
                ids.push_back(kv.first);
            }

            const std::size_t over = container.size() - cap;
            for (std::size_t i = 0; i < over; ++i) {
                ops::particle::remove<SampleEntity>(tx, ids[i]);
            }
        }
    };
    auto SampleEntity::Operations::from_float(float value_approximate) -> integer {
        const auto value = static_cast<integer>(value_approximate);
        return std::min(absolute_max, std::max(absolute_min, value));
    }
    auto SampleEntity::Operations::create_from_float(Writing gate, float field_value) -> Id {
        return ops::particle::create<SampleEntity>(gate, Quantum{from_float(field_value)});
    }
    auto SampleEntity::Operations::example_const_fieldwide_method(Reading world) -> integer {
        integer sum = integer{0};
        for (const auto& kv : world->field<SampleEntity>()->container) {
            sum = sum + kv.second->data_field;
        }
        return sum;
    }
    auto SampleEntity::Operations::example_const_element_method(Reading, Id) -> float { return 0.0f; }
    void SampleEntity::Operations::example_nonconst_fieldwide_method(Writing commit) {
        repo::Sequence tx{commit};
        const auto& container = tx->field<SampleEntity>()->container;
        if (container.empty()) return;

        auto it = container.begin();
        Id min_id = it->first;
        integer min_val = it->second->data_field;
        for (++it; it != container.end(); ++it) {
            const auto v = it->second->data_field;
            if (v < min_val) {
                min_val = v;
                min_id = it->first;
            }
        }
        ops::particle::remove<SampleEntity>(tx, min_id);
    }
    void SampleEntity::Operations::example_nonconst_element_method(Writing, Id) {}

    const Invariants SampleEntity::invariants{
        .structural = {},
        .logical = {
            invariant::for_each_item<SampleEntity, &SampleEntity_private::min_value>,
            invariant::for_each_item<SampleEntity, &SampleEntity_private::max_value>,
            &SampleEntity_private::some_logic_fieldwide_invariant,
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
        static auto construct(Writing commit, Id id, Item<Tag>) -> Quantum {
            return Quantum{
                .power = ops::particle::get<SampleEntity>(commit, id).data_field / ops::global::get<Tag>(commit)->modulus,
                .trivia = ops::particle::create<Trivia>(commit, Trivia::Quantum{}),
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

    const Invariants Remnant::invariants{
        .structural = {
            invariant::isomorphic<Tag, Remnant, &Remnant_private::construct>,
            invariant::anchor<Trivia, Remnant, &Remnant::Quantum::trivia>,
        },
        .logical = {
            invariant::for_each_item<Remnant, &Remnant_private::renmant_calculated>,
        },
    };

    //
    // SampleComponent
    struct SampleComponent_private : SampleComponent::Operations {
        static auto private_construct(Writing, Id, Item<SampleEntity>) -> Quantum { return {}; }
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
            invariant::anchor_any<SampleComponent, SampleAttribute, &SampleAttribute::Quantum::at_least_one_required>,
        },
        .logical = {
            invariant::existence<SampleAttribute, SampleEntity, &SampleAttribute_private::need_even, &SampleAttribute_private::construct>,
        },
    };

    auto SampleAttribute::Operations::create_complex_constructor(Writing commit, SampleEntity::Id existing) -> Id {
        const auto& existing_entity = ops::particle::get<SampleEntity>(commit, existing);

        auto data_field = existing_entity.data_field;
        if ((data_field % 2) != 0) data_field += 1;

        repo::Accumulator tx(commit);
        const auto created = ops::particle::create<SampleEntity>(tx, SampleEntity::Quantum{ data_field });
        ops::particle::create<SampleAttribute>(
            tx,
            created,
            SampleAttribute::Quantum{
                .neighbor_anchor = existing,
                .optional_anchor = {},
                .every_essential = {},
                .at_least_one_required = { existing },
            }
        );
        return created;
    }
}