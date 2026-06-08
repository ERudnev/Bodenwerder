#include "_common.h"

#include <optional>
#include <set>
#include <vector>

#include <fQSM/api/interface.h>
#include <fQSM/processing/context.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/world/data.h>
#include <fQSM/state/world/preview.h>

namespace {
    using namespace fqsm::api;

    namespace local {
        struct A : Entity<A> {
            struct Quantum { integer value; };
            static const Codex codex;
        };
    }

    namespace local {
        const A::Codex A::codex = {};
    }
}

namespace tests {

void delta_iterators()
{
    using namespace local;

    using Id = fqsm::Id<A>;

    const fqsm::Schema schema = fqsm::manipulation::schema::aspect<A>();

    fqsm::state::world::Data seed(schema);
    context::Realm fill(seed);

    std::vector<Id> ids;
    for (int i = 1; i <= 100; ++i) {
        ids.push_back(ask::item::create<A>(fill, {i}));
    }

    const fqsm::state::world::Data state(fill);
    auto patch = base::make_shared<fqsm::state::world::Patch>(schema);
    auto patch_context = std::make_shared<fqsm::processing::Context>(fqsm::processing::Context{
        state,
        patch,
        {}
    });
    auto writing = fqsm::processing::Gate{state, patch_context};

    std::vector<Id> added_ids;
    for (int i = 101; i <= 110; ++i) {
        added_ids.push_back(ask::item::create<A>(writing, {i * 10}));
    }
    for (int i = 0; i < 20; ++i) {
        ask::item::update<A>(writing, ids.at(i)).remove();
    }
    for (int i = 20; i < 50; ++i) {
        ask::item::update<A>(writing, ids.at(i))->value = (i + 1) * 10;
    }

    const fqsm::state::world::Preview preview(state, *patch);

    std::set<Id> all;
    std::set<Id> added;
    std::set<Id> removed;
    std::set<Id> updated;

    for (const auto change : preview.delta<A>()) {
        all.insert(change.id);
    }

    for (const auto change : preview.delta<A>().added()) {
        EXPECT_TRUE(change.add());
        EXPECT_FALSE(change.update());
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.before == nullptr);
        EXPECT_TRUE(change.after != nullptr);
        added.insert(change.id);
    }

    for (const auto change : preview.delta<A>().removed()) {
        EXPECT_FALSE(change.add());
        EXPECT_FALSE(change.update());
        EXPECT_TRUE(change.remove());
        EXPECT_TRUE(change.before != nullptr);
        EXPECT_TRUE(change.after == nullptr);
        removed.insert(change.id);
    }

    for (const auto change : preview.delta<A>().updated()) {
        EXPECT_FALSE(change.add());
        EXPECT_TRUE(change.update());
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.before != nullptr);
        EXPECT_TRUE(change.after != nullptr);
        updated.insert(change.id);
    }

    EXPECT_EQ(all.size(), std::size_t{60});
    EXPECT_EQ(added.size(), std::size_t{10});
    EXPECT_EQ(removed.size(), std::size_t{20});
    EXPECT_EQ(updated.size(), std::size_t{30});

    EXPECT_TRUE(all.contains(ids.at(0)));
    EXPECT_TRUE(all.contains(ids.at(20)));
    EXPECT_TRUE(all.contains(added_ids.at(0)));
    EXPECT_FALSE(all.contains(ids.at(50)));

    EXPECT_TRUE(added.contains(added_ids.at(0)));
    EXPECT_TRUE(added.contains(added_ids.at(9)));
    EXPECT_FALSE(added.contains(ids.at(99)));

    EXPECT_TRUE(removed.contains(ids.at(0)));
    EXPECT_TRUE(removed.contains(ids.at(19)));
    EXPECT_FALSE(removed.contains(ids.at(20)));

    EXPECT_TRUE(updated.contains(ids.at(20)));
    EXPECT_TRUE(updated.contains(ids.at(49)));
    EXPECT_FALSE(updated.contains(ids.at(50)));
}

} // namespace tests
