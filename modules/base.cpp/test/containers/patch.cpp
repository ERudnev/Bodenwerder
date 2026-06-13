#include "_common.h"

#include <base/containers/patch.h>

namespace tests {

void patch()
{
    using Element = base::patch::Element<int>;
    using Patch = base::Patch<int, Element>;

    Patch patch;

    EXPECT_TRUE(patch.empty());

    patch.insert(1, 10);
    patch.insert(2, std::nullopt);

    EXPECT_EQ(patch.size(), 2u);
    EXPECT_TRUE(patch.at(1).has_value());
    EXPECT_EQ(*patch.at(1), 10);
    EXPECT_FALSE(patch.at(2).has_value());

    patch.at(1) = 11;
    patch.at(2) = 20;

    for (auto entry : patch)
        if (entry.first == 1) entry.second = std::nullopt;

    EXPECT_FALSE(patch.at(1).has_value());
    EXPECT_TRUE(patch.at(2).has_value());
    EXPECT_EQ(*patch.at(2), 20);

    EXPECT_TRUE(patch.erase(2));
    EXPECT_FALSE(patch.contains(2));
}

} // namespace tests
