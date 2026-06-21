#include "_common.h"

#include <base/cannonball/denseTable.h>

#include <set>

namespace tests {

void denseTable()
{
    base::cannonball::DenseTable<int, int> table;

    EXPECT_TRUE(table.empty());

    table.insert(1, 10);
    table.insert(2, 20);
    table.insert(1, 11);

    for (auto entry : table)
        if (entry.id == 2) entry.value = 21;

    const auto& view = table;
    std::set<int> keys;
    for (const auto entry : view)
        keys.insert(entry.id);

    EXPECT_TRUE(keys == std::set<int>({1, 2}));
    EXPECT_EQ(table.size(), std::size_t{2});
    EXPECT_EQ(table.at(1), 11);
    EXPECT_EQ(table.at(2), 21);

    EXPECT_TRUE(table.erase(1));
    EXPECT_FALSE(table.contains(1));
    EXPECT_EQ(table.size(), std::size_t{1});
    EXPECT_EQ(table.at(2), 21);
}

} // namespace tests
