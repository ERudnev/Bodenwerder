#pragma once

#include <iQSM/_forwards.h>
#include <iQSM/internals/fields_mutable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/meta/global.h>
#include <iQSM/repository/commit.h>

namespace iqsm::repo {
    // Scope-based staging buffer:
    // - accumulate small atomic operations cheaply
    // - push a single baked Delta on scope exit
    struct Staged final {
        Commit& commit;
        internals::FieldsMutable staged;

        explicit Staged(Commit& c) : commit(c) {}

        ~Staged() {
            if (!staged.empty()) {
                commit.push(staged.push());
            }
        }

        template<meta::Aspect Meta>
        void add(Id<Meta> id, Item<Meta> after) {
            using Op = typename delta::FieldDiff<Meta>::Operation;
            staged.add_op<Meta>(std::move(id), Op{std::nullopt, std::move(after)});
        }

        template<meta::Aspect Meta>
        void remove(Id<Meta> id, Item<Meta> before) {
            using Op = typename delta::FieldDiff<Meta>::Operation;
            staged.add_op<Meta>(std::move(id), Op{std::move(before), std::nullopt});
        }

        template<meta::Aspect Meta>
        void update(Id<Meta> id, Item<Meta> before, Item<Meta> after) {
            using Op = typename delta::FieldDiff<Meta>::Operation;
            staged.add_op<Meta>(std::move(id), Op{std::move(before), std::move(after)});
        }

        template<meta::Aspect Meta>
        void set_global(typename ::iqsm::meta::Global<Meta> before, typename ::iqsm::meta::Global<Meta> after) {
            staged.set_global<Meta>(std::move(before), std::move(after));
        }
    };
}

