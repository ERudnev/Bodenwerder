#include "_common.h"

#include <base/cannonball/delta/operational.h>
#include <base/cannonball/table.h>
#include <base/cannonball/patch.h>

#include <set>

namespace tests {

void delta_demonstration()
{
    using Table = base::cannonball::Table<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Delta = base::cannonball::delta::Operational<int, int>;

    Table state;
    state.insert(1, 10);
    state.insert(2, 20);
    state.insert(3, 30);

    Patch patch;
    patch.modify(2, 200);
    patch.insert(3, base::cannonball::Patchlet<int>::deletion(30));
    patch.modify(4, 40);

    const Delta delta{state, patch};

    std::set<int> added;
    for (const auto change : delta.added()) {
        EXPECT_EQ(change.id, 4);
        EXPECT_EQ(change.now, 40);
        added.insert(change.id);
    }

    std::set<int> addedOrUpdated;
    for (const auto change : delta.addedOrUpdated()) {
        if (change.id == 2) {
            EXPECT_TRUE(change.old != nullptr);
            EXPECT_EQ(*change.old, 20);
            EXPECT_EQ(change.now, 200);
        }

        if (change.id == 4) {
            EXPECT_TRUE(change.old == nullptr);
            EXPECT_EQ(change.now, 40);
        }

        addedOrUpdated.insert(change.id);
    }

    std::set<int> removed;
    for (const auto change : delta.removed()) {
        EXPECT_EQ(change.id, 3);
        EXPECT_EQ(change.old, 30);
        removed.insert(change.id);
    }

    std::set<int> updated;
    for (const auto change : delta.updated()) {
        EXPECT_EQ(change.id, 2);
        EXPECT_EQ(change.old, 20);
        EXPECT_EQ(change.now, 200);
        updated.insert(change.id);
    }

    EXPECT_TRUE(added == std::set<int>({4}));
    EXPECT_TRUE(addedOrUpdated == std::set<int>({2, 4}));
    EXPECT_TRUE(removed == std::set<int>({3}));
    EXPECT_TRUE(updated == std::set<int>({2}));
}

} // namespace tests
