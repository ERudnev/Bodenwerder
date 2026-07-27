#include "_common.h"

#include <base/cannonball/patch.h>

namespace tests {

void patch_operations()
{
    using Patch = base::cannonball::Patch<int, int>;
    using Patchlet = base::cannonball::Patchlet<int>;
    using Table = base::cannonball::Table<int, int>;

    Patch patch;
    patch.modify(1, 10);
    patch.modify(2, 20);
    patch.insert(3, Patchlet::deletion(30));

    EXPECT_TRUE(patch.contains(1));
    EXPECT_FALSE(patch.at(1).tombstone);
    EXPECT_EQ(patch.at(1).quantum, 10);

    EXPECT_TRUE(patch.contains(3));
    EXPECT_TRUE(patch.at(3).tombstone);

    // Honest modify may touch quantum; tombstone stays.
    patch.modify(3, 30);
    EXPECT_TRUE(patch.at(3).tombstone);
    EXPECT_EQ(patch.at(3).quantum, 30);

    // Soft insert: modification does not clear tombstone (OR).
    patch.insert(3, Patchlet::modification(30));
    EXPECT_TRUE(patch.at(3).tombstone);
    EXPECT_EQ(patch.at(3).quantum, 30);

    // Intentional reincarnation: drop deletion, then insert live.
    patch.discard_changes(3);
    patch.insert(3, Patchlet::modification(30));
    EXPECT_FALSE(patch.at(3).tombstone);
    EXPECT_EQ(patch.at(3).quantum, 30);

    patch.discard_changes(2);
    EXPECT_FALSE(patch.contains(2));

    Table state;
    state.insert(1, 1);
    state.insert(2, 2);
    state.insert(4, 4);

    patch.insert(2, Patchlet::deletion(2));
    patch.modify(4, 40);

    Patch::integrate(state, patch);

    EXPECT_EQ(state.at(1), 10);
    EXPECT_FALSE(state.contains(2));
    EXPECT_EQ(state.at(3), 30);
    EXPECT_EQ(state.at(4), 40);

    Patch receiver;
    receiver.insert(7, Patchlet::deletion(70));

    Patch other;
    other.modify(7, 70);
    other.modify(8, 80);

    Patch::merge(receiver, other);

    EXPECT_TRUE(receiver.contains(7));
    EXPECT_TRUE(receiver.at(7).tombstone);
    EXPECT_FALSE(receiver.at(8).tombstone);
    EXPECT_EQ(receiver.at(8).quantum, 80);
}

} // namespace tests
