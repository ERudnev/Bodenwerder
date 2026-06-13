#include "_common.h"

#include <base/containers/table.h>

namespace tests {

void table()
{
    base::Table<int, int> table;

    EXPECT_TRUE(table.empty());

    table.insert(1, 10);
    table.insert(2, 20);
    table.insert(1, 11);

    for (auto entry : table)
        if (entry.first == 2) entry.second = 21;

    EXPECT_EQ(table.size(), 2u);
    EXPECT_EQ(table.at(1), 11);
    EXPECT_EQ(table.at(2), 21);
    EXPECT_TRUE(table.erase(1));
    EXPECT_FALSE(table.contains(1));
}

} // namespace tests
