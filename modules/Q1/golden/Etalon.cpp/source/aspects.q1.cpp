#include <Etalon/aspects.q1.h>

namespace Q1CORE::Etalon {

    using namespace iqsm::dsl_gateway;

    //
    // SampleEntity
    struct SampleEntity_private : SampleEntity::Operations {
        
        // construct name is soft-keyword. Naming operation as "construct" making binding easier
        static auto construct(Writing commit, Quantum value) -> Id {
            return ops::particle::create<SampleEntity>(commit, value);
        }

        static auto example_private_const_fieldwide_method(Reading) -> string { return {}; }
        static auto example_private_const_element_method(Reading, Id) -> float { return 0.0f; }

        static void example_private_nonconst_fieldwide_method(Writing) {}
        static void example_private_nonconst_element_method(Writing, Id) {}

        static auto example_private_validate_item_atomic(Reading, Id, const Quantum&) -> ItemChange { return {}; }

        static void example_private_validate_table(Writing) {}
    };
    auto SampleEntity::Operations::example_const_fieldwide_method(Reading) -> string { return {}; }
    auto SampleEntity::Operations::example_const_element_method(Reading, Id) -> float { return 0.0f; }
    void SampleEntity::Operations::example_nonconst_fieldwide_method(Writing) {}
    void SampleEntity::Operations::example_nonconst_element_method(Writing, Id) {}    

    const Invariants SampleEntity::invariants{
        .structural = {},
        .logical = {
            invariant::for_each_item<SampleEntity, &SampleEntity_private::example_private_validate_item_atomic>,
            &SampleEntity_private::example_private_validate_table,
        },
    };

    //
    // Tag (exists for even SampleEntity::data_field)
    struct Tag_private : Tag::Operations {
        static void clamp_global(Writing commit) {
            auto g = ops::global::modifier<Tag>(commit);
            if (g->modulus > integer{0}) return;
            g->modulus = integer{1};
        }

        static bool need_even(Reading world, SampleEntity::Id, Item<SampleEntity> item) {
            const auto mod = ops::global::get<Tag>(world)->modulus;
            const auto safe = mod > integer{0} ? mod : integer{1};
            return (item->data_field % safe) == integer{0};
        }
    };

    const Invariants Tag::invariants{
        .structural = {
            invariant::anchor_attribute<SampleEntity, Tag>,
        },
        .logical = {
            &Tag_private::clamp_global,
            invariant::existence<Tag, SampleEntity, &Tag_private::need_even>,
        },
    };

    //
    // SampleComponent
    struct SampleComponent_private : SampleComponent::Operations {
        static auto private_construct(Reading, Id, Item<SampleEntity>) -> Quantum { return {}; }
    };
    
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