#include "_common.h"

#include <base/cannonball/table.h>

#include <cstddef>

namespace tests {

void cursor_value_semantics()
{
    using Table = base::cannonball::Table<int, int>;

    Table table;
    table.insert(1, 10);
    table.insert(2, 20);
    table.insert(3, 30);

    const Table& view = table;

    // Copying a cursor and advancing the copy does not move the original.
    auto original = view.begin();
    auto copy = original;
    ++copy;

    EXPECT_TRUE(original == view.begin());
    EXPECT_TRUE((*original).id == (*view.begin()).id);
    EXPECT_FALSE(copy == original);

    // Two cursors independently constructed at the same position compare equal.
    const auto again = view.begin();
    EXPECT_TRUE(again == original);

    // end == end.
    EXPECT_TRUE(view.end() == view.end());
    EXPECT_FALSE(view.begin() == view.end());

    // Advancing a cursor all the way through lands it on end().
    auto walker = view.begin();
    for (std::size_t i = 0; i < view.size(); ++i) ++walker;
    EXPECT_TRUE(walker == view.end());
}

} // namespace tests
