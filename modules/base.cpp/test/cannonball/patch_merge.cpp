#include "_common.h"

#include <base/cannonball/patch.h>

namespace tests {

void patch_merge()
{
    using Patch = base::cannonball::Patch<int, int>;

    Patch receiver;
    receiver.insert(1, std::nullopt);
    receiver.modify(2, 20);

    Patch other;
    other.modify(1, 10);
    other.modify(2, 21);
    other.modify(3, 30);

    Patch::merge(receiver, other);

    EXPECT_FALSE(receiver.at(1).has_value());
    EXPECT_EQ(receiver.at(2).value(), 21);
    EXPECT_EQ(receiver.at(3).value(), 30);
}

} // namespace tests
