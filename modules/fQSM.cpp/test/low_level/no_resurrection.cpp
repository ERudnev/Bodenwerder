#include "_common.h"

#include <base/cannonball/patch.h>
#include <base/cannonball/table.h>

namespace tests {

// Invariant: a deletion patchlet is not a quantum to modify.
// Same path as QuantumGate::open_patchlet → update_modification → modify_modification
// after put_deletion / remove — must not resurrect. Today: hard crash (Tommy).
void no_resurrection()
{
    using Key = int;
    using Val = int;
    using Patch = base::cannonball::Patch<Key, Val>;
    using Table = base::cannonball::Table<Key, Val>;

    Table state;
    state.insert(1, 100);

    Patch patch;
    patch.insert(1, std::nullopt);

    (void)patch.modify_modification(1, [&]() -> const Val& {
        return state.at(1);
    });
}

} // namespace tests
