#include "_common.h"

#include <base/cannonball/patch.h>
#include <base/cannonball/table.h>

namespace tests {

// Invariant: tombstone survives soft modify; deletion patchlet still holds a quantum.
// Same path as QuantumGate → get_modification_access / modify_modification after put_deletion.
void no_resurrection()
{
    using Key = int;
    using Val = int;
    using Patch = base::cannonball::Patch<Key, Val>;
    using Patchlet = base::cannonball::Patchlet<Val>;
    using Table = base::cannonball::Table<Key, Val>;

    Table state;
    state.insert(1, 100);

    Patch patch;
    patch.insert(1, Patchlet::deletion(100));

    (void)patch.modify_modification(1, [&]() -> const Val& {
        return state.at(1);
    });
    EXPECT_TRUE(patch.at(1).tombstone);
}

} // namespace tests
