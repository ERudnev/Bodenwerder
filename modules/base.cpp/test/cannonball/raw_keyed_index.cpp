#include "_common.h"

#include <base/cannonball/patch.h>
#include <base/cannonball/table.h>

#include <cstdint>
#include <set>

namespace tests {

namespace {

// Mimics fqsm::Identifier<Meta>: distinct type, same underlying raw() value type.
struct MetaKey {
    explicit MetaKey(std::uint64_t v) : value(v) {}
    std::uint64_t raw() const { return value; }
    std::uint64_t value;
};

} // namespace

void raw_keyed_index()
{
    using Table = base::cannonball::Table<MetaKey, int>;

    Table table;
    table.insert(MetaKey{1}, 10);
    table.insert(MetaKey{2}, 20);

    EXPECT_TRUE(table.contains(MetaKey{1}));
    EXPECT_EQ(table.at(MetaKey{1}), 10);

    // Two distinct key objects with the same raw value are the same key.
    EXPECT_TRUE(table.contains(MetaKey{2}));
    EXPECT_EQ(*table.find(MetaKey{2}), 20);

    table.insert(MetaKey{2}, 21);
    EXPECT_EQ(table.size(), std::size_t{2});
    EXPECT_EQ(table.at(MetaKey{2}), 21);

    EXPECT_TRUE(table.erase(MetaKey{1}));
    EXPECT_FALSE(table.contains(MetaKey{1}));
    EXPECT_EQ(table.size(), std::size_t{1});

    std::set<std::uint64_t> seen;
    for (const auto entry : table)
        seen.insert(entry.id.raw());
    EXPECT_TRUE(seen == std::set<std::uint64_t>({2}));

    using Patch = base::cannonball::Patch<MetaKey, int>;
    using Patchlet = base::cannonball::Patchlet<int>;

    Patch patch;
    patch.modify(MetaKey{5}, 50);
    EXPECT_TRUE(patch.contains(MetaKey{5}));
    EXPECT_EQ(patch.at(MetaKey{5}).quantum, 50);

    // Same raw value, different key object: replaces the same patchlet, not a new one.
    patch.insert(MetaKey{5}, Patchlet::deletion(50));
    EXPECT_TRUE(patch.at(MetaKey{5}).tombstone);
    EXPECT_EQ(patch.size(), std::size_t{1});
}

} // namespace tests
