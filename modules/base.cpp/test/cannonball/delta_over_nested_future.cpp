#include "_common.h"

#include <base/cannonball/delta/interface.h>
#include <base/cannonball/future.h>
#include <base/cannonball/patch.h>
#include <base/cannonball/table.h>

namespace tests {

// DeltaCursor composes its state cursor through Read::begin()/end() like anything else, so a
// dirty-mode Delta whose `state` is itself a Future must see values through the nested overlay,
// not the raw root table.
void delta_over_nested_future()
{
    using Table = base::cannonball::Table<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Patchlet = base::cannonball::Patchlet<int>;
    using Future = base::cannonball::Future<int, int>;
    using Mode = base::cannonball::SeeChanges;
    using Delta = base::cannonball::delta::Delta<int, int>;
    using DeltaMode = base::cannonball::delta::Mode;

    Table table;
    table.insert(1, 10);
    table.insert(2, 20);

    Patch p1;
    p1.insert(1, Patchlet::modification(11)); // advancing layer patches id 1

    Future advancing(table, p1, Mode::observable);

    Patch p2;
    p2.insert(2, Patchlet::modification(22)); // proposal layer patches id 2 only

    // Dirty-mode delta whose state is `advancing` (a Future), not the root table.
    const Delta dirty{advancing, p2, DeltaMode::dirty};

    bool sawId1Tainted = false;
    bool sawId2Updated = false;

    for (const auto change : dirty) {
        if (change.id == 1) {
            // Untouched by p2, but must reflect advancing's overlay (11), not the raw table (10).
            EXPECT_TRUE(change.tainted());
            EXPECT_TRUE(change.after != nullptr);
            EXPECT_EQ(*change.after, 11);
            sawId1Tainted = true;
        }
        if (change.id == 2) {
            EXPECT_TRUE(change.update());
            EXPECT_EQ(*change.before.value(), 20); // advancing does not touch id 2
            EXPECT_EQ(*change.after, 22);
            sawId2Updated = true;
        }
    }

    EXPECT_TRUE(sawId1Tainted);
    EXPECT_TRUE(sawId2Updated);
}

} // namespace tests
