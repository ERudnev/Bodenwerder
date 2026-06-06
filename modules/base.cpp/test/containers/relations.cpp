#include "_common.h"

#include <base/containers/relations.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace {
    using Tested = base::Relations<int, int>;
    using Pair = std::pair<int, int>;

    auto collect(auto range) -> std::vector<Pair>
    {
        std::vector<Pair> out;
        out.reserve(range.size());

        for (const auto& link : range) {
            out.emplace_back(link.a, link.b);
        }

        std::sort(out.begin(), out.end());
        return out;
    }

    auto expected(std::initializer_list<Pair> init) -> std::vector<Pair>
    {
        std::vector<Pair> out(init);
        std::sort(out.begin(), out.end());
        return out;
    }
}

namespace tests {
    void relations()
    {
        Tested relations;

        EXPECT_TRUE(relations.empty());
        EXPECT_EQ(relations.size(), 0u);
        EXPECT_FALSE(relations.contains(1, 10));
        EXPECT_TRUE(collect(relations.find_a(1)) == expected({}));
        EXPECT_TRUE(collect(relations.find_b(10)) == expected({}));

        EXPECT_TRUE(relations.insert(1, 10));
        EXPECT_FALSE(relations.empty());
        EXPECT_EQ(relations.size(), 1u);
        EXPECT_TRUE(relations.contains(1, 10));
        EXPECT_TRUE(collect(relations.find_a(1)) == expected({ {1, 10} }));
        EXPECT_TRUE(collect(relations.find_b(10)) == expected({ {1, 10} }));

        EXPECT_FALSE(relations.insert(1, 10));
        EXPECT_EQ(relations.size(), 1u);

        EXPECT_TRUE(relations.insert(1, 11));
        EXPECT_TRUE(relations.insert(2, 10));
        EXPECT_TRUE(relations.insert(2, 12));
        EXPECT_EQ(relations.size(), 4u);
        EXPECT_TRUE(collect(relations.find_a(1)) == expected({ {1, 10}, {1, 11} }));
        EXPECT_TRUE(collect(relations.find_a(2)) == expected({ {2, 10}, {2, 12} }));
        EXPECT_TRUE(collect(relations.find_b(10)) == expected({ {1, 10}, {2, 10} }));
        EXPECT_TRUE(collect(relations.find_b(11)) == expected({ {1, 11} }));
        EXPECT_TRUE(collect(relations.find_b(12)) == expected({ {2, 12} }));

        EXPECT_TRUE(relations.erase(1, 10));
        EXPECT_FALSE(relations.contains(1, 10));
        EXPECT_EQ(relations.size(), 3u);
        EXPECT_TRUE(collect(relations.find_a(1)) == expected({ {1, 11} }));
        EXPECT_TRUE(collect(relations.find_b(10)) == expected({ {2, 10} }));

        EXPECT_FALSE(relations.erase(1, 10));
        EXPECT_EQ(relations.size(), 3u);

        relations.erase_a(1);
        EXPECT_FALSE(relations.contains(1, 11));
        EXPECT_EQ(relations.size(), 2u);
        EXPECT_TRUE(collect(relations.find_a(1)) == expected({}));
        EXPECT_TRUE(collect(relations.find_b(11)) == expected({}));
        EXPECT_TRUE(collect(relations.find_a(2)) == expected({ {2, 10}, {2, 12} }));

        EXPECT_TRUE(relations.insert(3, 12));
        EXPECT_TRUE(relations.insert(4, 12));
        EXPECT_EQ(relations.size(), 4u);
        EXPECT_TRUE(collect(relations.find_b(12)) == expected({ {2, 12}, {3, 12}, {4, 12} }));

        relations.erase_b(12);
        EXPECT_EQ(relations.size(), 1u);
        EXPECT_TRUE(relations.contains(2, 10));
        EXPECT_FALSE(relations.contains(2, 12));
        EXPECT_FALSE(relations.contains(3, 12));
        EXPECT_FALSE(relations.contains(4, 12));
        EXPECT_TRUE(collect(relations.find_a(2)) == expected({ {2, 10} }));
        EXPECT_TRUE(collect(relations.find_b(12)) == expected({}));
    }
}
