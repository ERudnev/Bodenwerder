#include "_common.h"

#include <base/cannonball/table.h>
#include <base/cannonball/patch.h>

namespace tests {

void patch_integrate()
{
    using Table = base::cannonball::Table<int, int>;
    using Patch = base::cannonball::Patch<int, int>;

    Table state;
    state.insert(1, 10);
    state.insert(2, 20);

    Patch patch;
    patch.modify(1, 11);
    patch.insert(2, std::nullopt);
    patch.modify(3, 30);

    Patch::integrate(state, patch);

    EXPECT_EQ(state.at(1), 11);
    EXPECT_FALSE(state.contains(2));
    EXPECT_EQ(state.at(3), 30);
}

} // namespace tests
