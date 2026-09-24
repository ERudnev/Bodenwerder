#include "_common.h"

#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>

namespace tests {

// Invariant: a tombstone survives touch and later modify-style writes; the deletion patchlet still holds a value.
// Same path as QuantumGate -> get_modification_access after put_deletion.
void no_resurrection()
{
    using namespace fqsm::erased;

    Line state(ops_of<int>(), ops_of<int>());
    const int stored = 100;
    state.insert(1, &stored);

    PatchLine patch(ops_of<int>(), ops_of<int>());
    patch.del(1, state.find(1));

    *static_cast<int*>(patch.touch(1, state.find(1))) = 101;
    EXPECT_TRUE(patch.mention(1).tombstone);

    const int written = 102;
    patch.modify(1, &written);
    EXPECT_TRUE(patch.mention(1).tombstone);
    EXPECT_EQ(*static_cast<const int*>(patch.mention(1).value), 102);
}

} // namespace tests
