#include "_common.h"

#include <base/cannonball/delta/interface.h>
#include <base/cannonball/table.h>
#include <base/cannonball/patch.h>

#include <set>

namespace tests {

void delta_dirty_mode()
{
    using Table = base::cannonball::Table<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Patchlet = base::cannonball::Patchlet<int>;
    using Delta = base::cannonball::delta::Delta<int, int>;
    using Mode = base::cannonball::delta::Mode;

    Table state;
    state.insert(1, 10);
    state.insert(2, 20);
    state.insert(3, 30);

    // In-place mutation of a state value, bypassing the patch entirely.
    if (auto* value = state.find(1)) *value = 15;

    Patch patch;
    patch.modify(2, 200);                     // modification
    patch.insert(3, Patchlet::deletion(30));   // deletion tombstone
    patch.modify(4, 40);                       // new id

    const Delta dirty{state, patch, Mode::dirty};
    const Delta clean{state, patch, Mode::clean};

    std::set<int> dirtyAdded, dirtyUpdated, dirtyRemoved, dirtyAddedOrUpdated;
    for (const auto change : dirty.added()) dirtyAdded.insert(change.id);
    for (const auto change : dirty.updated()) dirtyUpdated.insert(change.id);
    for (const auto change : dirty.removed()) dirtyRemoved.insert(change.id);
    for (const auto change : dirty.addedOrUpdated()) dirtyAddedOrUpdated.insert(change.id);

    std::set<int> cleanAdded, cleanUpdated, cleanRemoved, cleanAddedOrUpdated;
    for (const auto change : clean.added()) cleanAdded.insert(change.id);
    for (const auto change : clean.updated()) cleanUpdated.insert(change.id);
    for (const auto change : clean.removed()) cleanRemoved.insert(change.id);
    for (const auto change : clean.addedOrUpdated()) cleanAddedOrUpdated.insert(change.id);

    // Clean mode only sees explicit patch entries.
    EXPECT_TRUE(cleanAdded == std::set<int>({4}));
    EXPECT_TRUE(cleanUpdated == std::set<int>({2}));
    EXPECT_TRUE(cleanRemoved == std::set<int>({3}));
    EXPECT_TRUE(cleanAddedOrUpdated == std::set<int>({2, 4}));

    // Dirty mode agrees with clean mode on the explicitly-patched ids...
    EXPECT_TRUE(dirtyAdded == cleanAdded);
    EXPECT_TRUE(dirtyUpdated == cleanUpdated);
    EXPECT_TRUE(dirtyRemoved == cleanRemoved);

    // ...but additionally reports the untouched-by-patch, live-mutated state
    // entry (id 1) as tainted: visible in addedOrUpdated()/all(), never in
    // added() or updated() specifically (no prior patch entry to compare against).
    EXPECT_FALSE(dirtyAdded.contains(1));
    EXPECT_FALSE(dirtyUpdated.contains(1));
    EXPECT_TRUE(dirtyAddedOrUpdated == std::set<int>({1, 2, 4}));

    std::set<int> dirtyAll;
    for (const auto change : dirty) dirtyAll.insert(change.id);
    EXPECT_TRUE(dirtyAll == std::set<int>({1, 2, 3, 4}));

    for (const auto change : dirty) {
        if (change.id == 1) {
            EXPECT_TRUE(change.tainted());
            EXPECT_EQ(*change.after, 15);
        }
    }
}

} // namespace tests
