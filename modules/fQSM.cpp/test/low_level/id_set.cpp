#include "_common.h"

#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

#include <fQSM/id_set.h>
#include <fQSM/identifier.h>

// IdSet against std::unordered_set under random insert, erase and lookup, including the zero id,
// growth, backward-shift erase inside probe runs, copies and equality.
namespace {
    struct Tag {};
    using Id = fqsm::Identifier<Tag>;
}

namespace tests {

void id_set()
{
    fqsm::IdSet<Id> set;
    std::unordered_set<std::uint64_t> model;

    EXPECT_TRUE(set.empty());
    EXPECT_TRUE(set.begin() == set.end());
    EXPECT_FALSE(set.contains(Id{0}));

    // clustered keys: many collide under any hash, so erase must shift runs correctly
    std::mt19937_64 rng(42);
    std::vector<std::uint64_t> keys;
    for (std::uint64_t i = 0; i < 64; ++i) keys.push_back(i);                 // includes 0
    for (int i = 0; i < 2000; ++i) keys.push_back(rng());
    for (int i = 0; i < 500; ++i) keys.push_back((rng() & 0xffff) * 4096);    // aligned, collision-prone

    for (int round = 0; round < 20000; ++round) {
        const auto key = keys[rng() % keys.size()];
        const int op = static_cast<int>(rng() % 3);
        if (op < 2) {
            const bool fresh = set.insert(Id{key}).second;
            EXPECT_EQ(fresh, model.insert(key).second);
        } else {
            EXPECT_EQ(set.erase(Id{key}), model.erase(key));
        }
        if (round % 997 == 0) {
            EXPECT_EQ(set.size(), model.size());
            std::size_t seen = 0;
            for (const auto id : set) {
                EXPECT_TRUE(model.contains(id.raw()));
                ++seen;
            }
            EXPECT_EQ(seen, model.size());
        }
    }
    EXPECT_EQ(set.size(), model.size());
    for (const auto key : keys)
        EXPECT_EQ(set.contains(Id{key}), model.contains(key));
    for (const auto key : keys)
        EXPECT_EQ(set.find(Id{key}) == set.end(), not model.contains(key));

    // a copy is equal and independent
    const fqsm::IdSet<Id> copy = set;
    EXPECT_TRUE(copy == set);
    EXPECT_EQ(copy.size(), set.size());
    fqsm::IdSet<Id> changed = set;
    const auto some = *set.begin();
    changed.erase(some);
    EXPECT_FALSE(changed == set);
    EXPECT_EQ(changed.size() + 1, set.size());

    // the zero id
    fqsm::IdSet<Id> zero;
    EXPECT_TRUE(zero.insert(Id{0}).second);
    EXPECT_FALSE(zero.insert(Id{0}).second);
    EXPECT_TRUE(zero.contains(Id{0}));
    EXPECT_EQ(zero.size(), std::size_t{1});
    std::size_t seen = 0;
    for (const auto id : zero) { EXPECT_EQ(id.raw(), std::uint64_t{0}); ++seen; }
    EXPECT_EQ(seen, std::size_t{1});
    EXPECT_EQ(zero.erase(Id{0}), std::size_t{1});
    EXPECT_TRUE(zero.empty());

    // clear keeps the capacity and drops every element
    set.clear();
    EXPECT_TRUE(set.empty());
    EXPECT_TRUE(set.begin() == set.end());
    EXPECT_TRUE(set.insert(Id{7}).second);
    EXPECT_EQ(set.size(), std::size_t{1});

    // initializer list and reserve
    fqsm::IdSet<Id> three{Id{1}, Id{2}, Id{3}};
    EXPECT_EQ(three.size(), std::size_t{3});
    three.reserve(10000);
    EXPECT_EQ(three.size(), std::size_t{3});
    EXPECT_TRUE(three.contains(Id{2}));
}

} // namespace tests
