#include "_common.h"

#include <base/cannonball/patch.h>

#include <string>

namespace tests {

void patch_modify_modification()
{
    using Val = std::string;
    using Patch = base::cannonball::Patch<std::string, Val>;

    Patch patch;
    const std::string id = "alpha";
    const std::string prepatchValue = "prepatch";

    bool prepatchCalled = false;
    Val& first = patch.modify_modification(id, [&]() -> const Val& {
        prepatchCalled = true;
        return prepatchValue;
    });

    EXPECT_TRUE(prepatchCalled);
    EXPECT_EQ(first, "prepatch");
    EXPECT_TRUE(patch.contains(id));
    EXPECT_FALSE(patch.at(id).verified);
    EXPECT_EQ(patch.at(id).quantum, "prepatch");

    bool prepatchCalledAgain = false;
    Val& second = patch.modify_modification(id, [&]() -> const Val& {
        prepatchCalledAgain = true;
        return prepatchValue;
    });

    EXPECT_FALSE(prepatchCalledAgain);
    EXPECT_EQ(second, "prepatch");
    EXPECT_EQ(std::addressof(second), std::addressof(patch.at(id).quantum));
}

} // namespace tests
