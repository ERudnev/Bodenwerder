#include "_common.h"

#include <cstdint>
#include <string>

#include <fQSM/erased/slots.h>

#include "counted.h"

#ifdef _MSC_VER
#pragma warning(disable: 4324)
#endif

namespace {
    struct alignas(32) Wide {
        std::uint64_t value;
    };
}

namespace tests {

void erased_slots_basic()
{
    using fqsm::erased::Slots;
    Slots slots(fqsm::erased::ops_of<int>());
    EXPECT_TRUE(slots.empty());

    for (int i = 0; i < 100; ++i) {
        const auto slot = slots.push_copy(&i);
        EXPECT_EQ(slot, static_cast<Slots::Index>(i));
    }
    EXPECT_EQ(slots.size(), std::size_t{100});
    EXPECT_EQ(*static_cast<const int*>(slots.at(42)), 42);

    EXPECT_TRUE(slots.release(10));
    EXPECT_EQ(slots.size(), std::size_t{99});
    EXPECT_EQ(*static_cast<const int*>(slots.at(10)), 99);

    EXPECT_FALSE(slots.release(98));
    EXPECT_EQ(slots.size(), std::size_t{98});

    const auto fresh = slots.push_default();
    EXPECT_EQ(*static_cast<const int*>(slots.at(fresh)), 0);

    slots.clear();
    EXPECT_TRUE(slots.empty());
}

void erased_slots_aligned()
{
    fqsm::erased::Slots slots(fqsm::erased::ops_of<Wide>());
    EXPECT_EQ(slots.stride(), std::size_t{32});
    for (std::uint64_t i = 0; i < 17; ++i) {
        const Wide value{i};
        slots.push_copy(&value);
    }
    for (std::uint32_t i = 0; i < 17; ++i) {
        const auto address = reinterpret_cast<std::uintptr_t>(slots.at(i));
        EXPECT_EQ(address % 32, std::uintptr_t{0});
        EXPECT_EQ(static_cast<const Wide*>(slots.at(i))->value, std::uint64_t{i});
    }
}

void erased_slots_lifetime()
{
    using erased::Counted;
    using erased::Counts;

    Counts counts;
    {
        fqsm::erased::Slots slots(fqsm::erased::ops_of<Counted>());
        slots.reserve(8);

        Counted a(counts, "a");
        Counted b(counts, "b");
        slots.push_copy(&a);
        slots.push_move(&b);
        EXPECT_EQ(counts.copied, 1);
        EXPECT_EQ(counts.moved, 1);
        EXPECT_EQ(static_cast<const Counted*>(slots.at(1))->text, std::string("b"));

        slots.push_copy(slots.at(0));
        EXPECT_EQ(static_cast<const Counted*>(slots.at(2))->text, std::string("a"));

        const int destroyedBefore = counts.destroyed;
        EXPECT_TRUE(slots.release(0));
        EXPECT_EQ(counts.destroyed, destroyedBefore + 2);
        EXPECT_EQ(slots.size(), std::size_t{2});
        EXPECT_EQ(static_cast<const Counted*>(slots.at(0))->text, std::string("a"));

        for (int i = 0; i < 20; ++i)
            slots.push_copy(&a);
        EXPECT_EQ(counts.alive(), 2 + 22);

        slots.push_copy(slots.at(21));
        EXPECT_EQ(static_cast<const Counted*>(slots.at(22))->text, std::string("a"));
        EXPECT_TRUE(slots.release(22) == false);

        fqsm::erased::Slots copy(slots);
        EXPECT_EQ(copy.size(), slots.size());
        EXPECT_EQ(counts.alive(), 2 + 44);

        fqsm::erased::Slots moved(std::move(copy));
        EXPECT_EQ(moved.size(), std::size_t{22});
        EXPECT_EQ(counts.alive(), 2 + 44);

        slots.clear();
        EXPECT_EQ(counts.alive(), 2 + 22);
    }
    EXPECT_EQ(counts.alive(), 0);
}

} // namespace tests
