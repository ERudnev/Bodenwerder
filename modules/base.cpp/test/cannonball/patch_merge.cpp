#include "_common.h"

#include <base/cannonball/patch.h>

namespace tests {

void patch_merge()
{
    using Patch = base::cannonball::Patch<int, int>;
    using Patchlet = base::cannonball::Patchlet<int>;

    Patch receiver;
    receiver.insert(1, Patchlet::deletion(10));
    receiver.modify(2, 20);

    Patch other;
    other.modify(1, 10);
    other.modify(2, 21);
    other.modify(3, 30);

    Patch::merge(receiver, other);

    EXPECT_TRUE(receiver.at(1).tombstone);
    EXPECT_EQ(receiver.at(2).quantum, 21);
    EXPECT_EQ(receiver.at(3).quantum, 30);
}

} // namespace tests
