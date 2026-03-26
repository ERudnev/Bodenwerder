#pragma once

#include <iQSM/_all.include.h>

namespace tests {
    using namespace iqsm::dsl_gateway;
}

namespace tests::utility {

    struct Foo : Entity<Foo>, Require<> {
        struct Quantum {
            integer value = 0;
        };
        static const Invariants invariants;
    };

    struct Bar : Attribute<Bar, Foo>, Require<Foo> {
        struct Quantum {};
        static const Invariants invariants;
    };

    namespace Foo_impl {
        inline void validate_nonnegative(iqsm::repo::Commit commit) {
            const auto world = commit.initial;
            const auto field = world->field<Foo>();

            iqsm::internals::FieldsMutable staged{};
            using Operation = typename iqsm::delta::FieldDiff<Foo>::Operation;

            for (const auto& [id, item] : field->container) {
                if (item->value >= integer{0}) continue;
                staged.add_op<Foo>(id, Operation{ item, std::nullopt });
            }

            if (not staged.empty()) {
                commit.push(staged.push());
            }
        }
    }

    inline const Invariants Foo::invariants{{{
        &Foo_impl::validate_nonnegative,
    }}};

    inline const Invariants Bar::invariants{{{
        invariant::anchor_attribute<Foo, Bar>,
    }}};
}
