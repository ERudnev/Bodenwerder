#include "_common.h"


#include <optional>
#include <set>

namespace tests {

void tableDelta()
{
    /*
    using State = base::Table<int, int>;
    using PatchElement = base::patch::Element<int>;
    using Patch = base::Patch<int, PatchElement>;
    State state;
    state.insert(1, 10);
    state.insert(2, 20);
    state.insert(3, 30);

    Patch patch;
    patch.insert(2, 200);
    patch.insert(3, std::nullopt);
    patch.insert(4, 40);
    patch.insert(5, std::nullopt);

    base::Draft<int, int> draft(state, patch);

    std::set<int> all;
    std::set<int> added;
    std::set<int> addedOrUpdated;
    std::set<int> removed;
    std::set<int> updated;

    for (const auto change : draft.delta()) {
        EXPECT_TRUE(change.good());
        all.insert(change.key);
    }

    for (const auto change : draft.delta().added()) {
        EXPECT_TRUE(change.add());
        EXPECT_FALSE(change.update());
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.before == nullptr);
        EXPECT_TRUE(change.after != nullptr);
        added.insert(change.key);
    }

    for (const auto change : draft.delta().addedOrUpdated()) {
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.after != nullptr);
        addedOrUpdated.insert(change.key);
    }

    for (const auto change : draft.delta().removed()) {
        EXPECT_FALSE(change.add());
        EXPECT_FALSE(change.update());
        EXPECT_TRUE(change.remove());
        EXPECT_TRUE(change.before != nullptr);
        EXPECT_TRUE(change.after == nullptr);
        removed.insert(change.key);
    }

    for (const auto change : draft.delta().updated()) {
        EXPECT_FALSE(change.add());
        EXPECT_TRUE(change.update());
        EXPECT_FALSE(change.remove());
        EXPECT_TRUE(change.before != nullptr);
        EXPECT_TRUE(change.after != nullptr);
        updated.insert(change.key);
    }

    EXPECT_TRUE(all == std::set<int>({2, 3, 4}));
    EXPECT_TRUE(added == std::set<int>({4}));
    EXPECT_TRUE(addedOrUpdated == std::set<int>({2, 4}));
    EXPECT_TRUE(removed == std::set<int>({3}));
    EXPECT_TRUE(updated == std::set<int>({2}));
    EXPECT_FALSE(all.contains(5));
    */
}

} // namespace tests
