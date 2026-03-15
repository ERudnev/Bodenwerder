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
        inline iqsm::Delta validate_nonnegative(iqsm::World world) {
            const auto field = world->field<Foo>();

            auto fd = base::make_shared<iqsm::delta::FieldDiff<Foo>>();

            for (const auto& [id, item] : field->container) {
                if (item->value >= integer{0}) continue;
                typename iqsm::delta::FieldDiff<Foo>::Operation op{};
                op.remove = true;
                fd->ops = fd->ops.insert(id, std::move(op));
            }

            if (fd->ops.empty()) return iqsm::delta::empty();

            auto wd = base::make_shared<iqsm::delta::Fields>();
            wd->fields = wd->fields.insert(iqsm::Facet<Foo>::typeId, iqsm::freeze(fd));
            return iqsm::freeze(wd);
        }
    }

    inline const Invariants Foo::invariants{{{
        &Foo_impl::validate_nonnegative,
    }}};

    inline const Invariants Bar::invariants{{{
        Invariants::anchor_attribute<Foo, Bar>,
    }}};
}
