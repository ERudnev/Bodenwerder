#include "_common.h"

#include <base/cannonball/denseTable.h>
#include <base/cannonball/patch.h>
#include <base/cannonball/preview.h>

#include <set>

namespace tests {

void preview_future_world()
{
    using Table = base::cannonball::DenseTable<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Preview = base::cannonball::Preview<int, int>;

    Table state;
    state.insert(1, 10);
    state.insert(2, 20);
    state.insert(3, 30);

    Patch patch;
    patch.modify(2, 200);
    patch.insert(3, std::nullopt);
    patch.modify(4, 40);

    Preview preview(state, patch);

    EXPECT_TRUE(preview.contains(1));
    EXPECT_TRUE(preview.contains(2));
    EXPECT_FALSE(preview.contains(3));
    EXPECT_TRUE(preview.contains(4));

    EXPECT_EQ(preview.at(1), 10);
    EXPECT_EQ(preview.at(2), 200);
    EXPECT_EQ(preview.at(4), 40);
    EXPECT_EQ(preview.size(), std::size_t{3});

    std::set<int> visibleKeys;
    for (const auto entry : preview)
        visibleKeys.insert(entry.id);

    EXPECT_TRUE(visibleKeys == std::set<int>({1, 2, 4}));
}

} // namespace tests
