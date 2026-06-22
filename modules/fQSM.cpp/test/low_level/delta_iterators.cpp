#include "_common.h"

#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include <fQSM/api/interface.h>
#include <fQSM/processing/contexts/operational.h>
#include <fQSM/model/complex/draft.h>

namespace {
    using namespace fqsm::api;

    namespace local {
        struct A : Entity<A> {
            struct Quantum { integer value; };
            static const Behavior behavior;
        };
    }

    namespace local {
        const A::Behavior A::behavior = {};
    }

    // temp lib. placeholder:
    inline const fqsm::model::complex::State& look(fqsm::Reading source) { return source; }
}

namespace tests {

void delta_iterators()
{
    using namespace local;
    using Id = fqsm::Id<A>;
    using Context = fqsm::processing::context::Operational;

    const fqsm::Schema schema = fqsm::manipulation::schema::aspect<A>();

    context::Realm fill(schema);

    std::vector<Id> ids;
    for (int i = 1; i <= 100; ++i) {
        ids.push_back(ask::item::create<A>(fill, {i}));
    }

    const fqsm::model::complex::Reality state(look(fill));
    auto patch = base::make_shared<fqsm::model::complex::Patch>(schema);
    auto patch_context = std::make_shared<Context>(Context{state, patch, {}});
    auto writing = fqsm::processing::Gate{patch_context};

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

    const fqsm::model::complex::Draft preview(state, patch);

    using Layer = fqsm::model::linear::Delta<A>::Layer;
    std::unordered_map<Layer, std::set<Id>> collected;

    for (const auto change : preview.delta<A>()) {
        collected[Layer::all].insert(change.id);
    }

    for (const auto change : preview.delta<A>().added()) {
        EXPECT_TRUE(change.add());
        EXPECT_FALSE(change.update());
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.before.has_value());
        EXPECT_TRUE(change.before.value() == nullptr);
        EXPECT_TRUE(change.after != nullptr);
        collected[Layer::added].insert(change.id);
    }

    for (const auto change : preview.delta<A>().addedOrUpdated()) {
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.after != nullptr);
        collected[Layer::addedOrUpdated].insert(change.id);
    }

    for (const auto change : preview.delta<A>().removed()) {
        EXPECT_FALSE(change.add());
        EXPECT_FALSE(change.update());
        EXPECT_TRUE(change.remove());
        EXPECT_TRUE(change.before.has_value());
        EXPECT_TRUE(change.before.value() != nullptr);
        EXPECT_TRUE(change.after == nullptr);
        collected[Layer::removed].insert(change.id);
    }

    for (const auto change : preview.delta<A>().updated()) {
        EXPECT_FALSE(change.add());
        EXPECT_TRUE(change.update());
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.before.has_value());
        EXPECT_TRUE(change.before.value() != nullptr);
        EXPECT_TRUE(change.after != nullptr);
        collected[Layer::updated].insert(change.id);
    }

    EXPECT_EQ(collected[Layer::all].size(), std::size_t{60});
    EXPECT_EQ(collected[Layer::added].size(), std::size_t{10});
    EXPECT_EQ(collected[Layer::addedOrUpdated].size(), std::size_t{40});
    EXPECT_EQ(collected[Layer::removed].size(), std::size_t{20});
    EXPECT_EQ(collected[Layer::updated].size(), std::size_t{30});

    EXPECT_TRUE(collected[Layer::all].contains(ids.at(0)));
    EXPECT_TRUE(collected[Layer::all].contains(ids.at(20)));
    EXPECT_TRUE(collected[Layer::all].contains(added_ids.at(0)));
    EXPECT_FALSE(collected[Layer::all].contains(ids.at(50)));

    EXPECT_TRUE(collected[Layer::added].contains(added_ids.at(0)));
    EXPECT_TRUE(collected[Layer::added].contains(added_ids.at(9)));
    EXPECT_FALSE(collected[Layer::added].contains(ids.at(99)));

    EXPECT_TRUE(collected[Layer::addedOrUpdated].contains(added_ids.at(0)));
    EXPECT_TRUE(collected[Layer::addedOrUpdated].contains(ids.at(20)));
    EXPECT_TRUE(collected[Layer::addedOrUpdated].contains(ids.at(49)));
    EXPECT_FALSE(collected[Layer::addedOrUpdated].contains(ids.at(0)));
    EXPECT_FALSE(collected[Layer::addedOrUpdated].contains(ids.at(50)));

    EXPECT_TRUE(collected[Layer::removed].contains(ids.at(0)));
    EXPECT_TRUE(collected[Layer::removed].contains(ids.at(19)));
    EXPECT_FALSE(collected[Layer::removed].contains(ids.at(20)));

    EXPECT_TRUE(collected[Layer::updated].contains(ids.at(20)));
    EXPECT_TRUE(collected[Layer::updated].contains(ids.at(49)));
    EXPECT_FALSE(collected[Layer::updated].contains(ids.at(50)));

    std::set<Id> expected_added_or_updated = collected[Layer::added];
    expected_added_or_updated.insert(collected[Layer::updated].begin(), collected[Layer::updated].end());
    EXPECT_EQ(collected[Layer::addedOrUpdated], expected_added_or_updated);
}

} // namespace tests
