#include "_common.h"

#include <base/cannonball/denseTable.h>
#include <base/cannonball/draft.h>

#include <set>

namespace tests {

void draft_patch_build()
{
    using Table = base::cannonball::DenseTable<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Draft = base::cannonball::Draft<int, int>;

    Table state;
    state.insert(1, 10);
    state.insert(2, 20);

    Patch patch;
    patch.insert(2, 200);
    patch.insert(4, 40);

    Draft draft(state, patch);

    EXPECT_TRUE(draft.contains(1));
    EXPECT_TRUE(draft.contains(2));
    EXPECT_TRUE(draft.contains(4));
    EXPECT_FALSE(draft.contains(3));

    EXPECT_EQ(draft.at(1), 10);
    EXPECT_EQ(draft.at(2), 200);
    EXPECT_EQ(draft.at(4), 40);
    EXPECT_EQ(draft.size(), std::size_t{3});

    std::set<int> visibleKeys;
    for (const auto entry : draft)
        visibleKeys.insert(entry.id);

    EXPECT_TRUE(visibleKeys == std::set<int>({1, 2, 4}));

    draft.insert(1, 11);
    draft.insert(3, 30);
    draft.erase(2);
    draft.erase(4);

    EXPECT_TRUE(patch.contains(1));
    EXPECT_TRUE(patch.at(1).has_value());
    EXPECT_EQ(patch.at(1).value(), 11);

    EXPECT_TRUE(patch.contains(2));
    EXPECT_FALSE(patch.at(2).has_value());

    EXPECT_TRUE(patch.contains(3));
    EXPECT_TRUE(patch.at(3).has_value());
    EXPECT_EQ(patch.at(3).value(), 30);

    EXPECT_FALSE(patch.contains(4));

    EXPECT_EQ(draft.at(1), 11);
    EXPECT_FALSE(draft.contains(2));
    EXPECT_TRUE(draft.contains(3));
    EXPECT_FALSE(draft.contains(4));
}

} // namespace tests
