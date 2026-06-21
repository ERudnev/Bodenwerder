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
    patch.insert(3, std::nullopt);

    EXPECT_TRUE(patch.contains(1));
    EXPECT_TRUE(patch.at(1).has_value());
    EXPECT_EQ(patch.at(1).value(), 10);

    EXPECT_TRUE(patch.contains(3));
    EXPECT_FALSE(patch.at(3).has_value());

    patch.modify(3, 30);
    EXPECT_FALSE(patch.at(3).has_value());

    patch.insert(3, Patchlet{30});
    EXPECT_TRUE(patch.at(3).has_value());
    EXPECT_EQ(patch.at(3).value(), 30);

    patch.discard_changes(2);
    EXPECT_FALSE(patch.contains(2));

    Table state;
    state.insert(1, 1);
    state.insert(2, 2);
    state.insert(4, 4);

    patch.insert(2, std::nullopt);
    patch.modify(4, 40);

    Patch::integrate(state, patch);

    EXPECT_EQ(state.at(1), 10);
    EXPECT_FALSE(state.contains(2));
    EXPECT_EQ(state.at(3), 30);
    EXPECT_EQ(state.at(4), 40);

    Patch receiver;
    receiver.insert(7, std::nullopt);

    Patch other;
    other.modify(7, 70);
    other.modify(8, 80);

    Patch::merge(receiver, other);

    EXPECT_TRUE(receiver.contains(7));
    EXPECT_FALSE(receiver.at(7).has_value());
    EXPECT_TRUE(receiver.at(8).has_value());
    EXPECT_EQ(receiver.at(8).value(), 80);
}

} // namespace tests
