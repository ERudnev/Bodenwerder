#include "_common.h"

#include <base/containers/relations.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace {
    using Tested = base::Relations<int, int>;
    using Values = std::vector<int>;

    auto sorted(std::vector<int> values) -> std::vector<int>
    {
        std::sort(values.begin(), values.end());
        return values;
    }
}

namespace tests {
    void relations()
    {
        Tested relations;

        EXPECT_TRUE(relations.empty());
        EXPECT_EQ(relations.size(), 0u);
        EXPECT_FALSE(relations.contains(1, 10));
        EXPECT_TRUE(relations.find_a(1) == Values{});
        EXPECT_TRUE(relations.find_b(10) == Values{});

        EXPECT_TRUE(relations.insert(1, 10));
        EXPECT_FALSE(relations.empty());
        EXPECT_EQ(relations.size(), 1u);
        EXPECT_TRUE(relations.contains(1, 10));
        EXPECT_TRUE(relations.find_a(1) == Values{ 10 });
        EXPECT_TRUE(relations.find_b(10) == Values{ 1 });

        EXPECT_FALSE(relations.insert(1, 10));
        EXPECT_EQ(relations.size(), 1u);

        EXPECT_TRUE(relations.insert(1, 11));
        EXPECT_TRUE(relations.insert(2, 10));
        EXPECT_TRUE(relations.insert(2, 12));
        EXPECT_EQ(relations.size(), 4u);
        EXPECT_TRUE(sorted(relations.find_a(1)) == Values({ 10, 11 }));
        EXPECT_TRUE(sorted(relations.find_a(2)) == Values({ 10, 12 }));
        EXPECT_TRUE(sorted(relations.find_b(10)) == Values({ 1, 2 }));
        EXPECT_TRUE(relations.find_b(11) == Values({ 1 }));
        EXPECT_TRUE(relations.find_b(12) == Values({ 2 }));

        EXPECT_TRUE(relations.erase(1, 10));
        EXPECT_FALSE(relations.contains(1, 10));
        EXPECT_EQ(relations.size(), 3u);
        EXPECT_TRUE(relations.find_a(1) == Values({ 11 }));
        EXPECT_TRUE(relations.find_b(10) == Values({ 2 }));

        EXPECT_FALSE(relations.erase(1, 10));
        EXPECT_EQ(relations.size(), 3u);

        relations.erase_a(1);
        EXPECT_FALSE(relations.contains(1, 11));
        EXPECT_EQ(relations.size(), 2u);
        EXPECT_TRUE(relations.find_a(1) == Values{});
        EXPECT_TRUE(relations.find_b(11) == Values{});
        EXPECT_TRUE(sorted(relations.find_a(2)) == Values({ 10, 12 }));

        EXPECT_TRUE(relations.insert(3, 12));
        EXPECT_TRUE(relations.insert(4, 12));
        EXPECT_EQ(relations.size(), 4u);
        EXPECT_TRUE(sorted(relations.find_b(12)) == Values({ 2, 3, 4 }));

        relations.erase_b(12);
        EXPECT_EQ(relations.size(), 1u);
        EXPECT_TRUE(relations.contains(2, 10));
        EXPECT_FALSE(relations.contains(2, 12));
        EXPECT_FALSE(relations.contains(3, 12));
        EXPECT_FALSE(relations.contains(4, 12));
        EXPECT_TRUE(relations.find_a(2) == Values({ 10 }));
        EXPECT_TRUE(relations.find_b(12) == Values{});
    }
}
