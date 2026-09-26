#include "_common.h"

#include <string>

#include <fQSM/erased/algorithms.h>
#include <fQSM/erased/patch_line.h>

#include "counted.h"
#include "patch_stub.h"

namespace {
    int value_of(const fqsm::erased::PatchLine& patch, fqsm::RawId id) {
        return *static_cast<const int*>(patch.mention(id).value);
    }

    bool tombstone(const fqsm::erased::PatchLine& patch, fqsm::RawId id) {
        return patch.mention(id).tombstone;
    }
}

namespace tests {

void erased_patch_line_operations()
{
    auto patch = erased::int_patch();
    EXPECT_FALSE(patch.has_changes());

    erased::put(patch, 1, 10);
    erased::put(patch, 2, 20);
    erased::put(patch, 3, 30, true);
    EXPECT_TRUE(patch.has_changes());
    EXPECT_EQ(patch.count(), std::size_t{3});
    EXPECT_FALSE(tombstone(patch, 1));
    EXPECT_EQ(value_of(patch, 1), 10);
    EXPECT_TRUE(tombstone(patch, 3));
    EXPECT_EQ(value_of(patch, 3), 30);
    EXPECT_FALSE(patch.mention(4).found);

    // honest modify replaces the value, the tombstone stays
    erased::put(patch, 3, 31);
    EXPECT_TRUE(tombstone(patch, 3));
    EXPECT_EQ(value_of(patch, 3), 31);

    // only discard plus a fresh write reincarnates
    EXPECT_TRUE(patch.discard(3));
    EXPECT_FALSE(patch.discard(3));
    erased::put(patch, 3, 32);
    EXPECT_FALSE(tombstone(patch, 3));
    EXPECT_EQ(value_of(patch, 3), 32);

    EXPECT_TRUE(patch.discard(1));
    EXPECT_FALSE(patch.mention(1).found);
    EXPECT_EQ(value_of(patch, 2), 20);
    EXPECT_EQ(value_of(patch, 3), 32);

    // deletion with the line's own value keeps it as the last value
    patch.del(2, patch.mention(2).value);
    EXPECT_TRUE(tombstone(patch, 2));
    EXPECT_EQ(value_of(patch, 2), 20);

    const int global = 7;
    patch.set_global(&global);
    EXPECT_EQ(*static_cast<const int*>(patch.global()), 7);

    patch.clear();
    EXPECT_FALSE(patch.has_changes());
    EXPECT_TRUE(patch.global() == nullptr);
}

void erased_patch_line_touch()
{
    auto patch = erased::int_patch();
    const int base = 5;

    void* first = patch.touch(1, &base);
    EXPECT_EQ(*static_cast<int*>(first), 5);
    EXPECT_FALSE(patch.verified_at(0));
    *static_cast<int*>(first) = 6;

    const int other = 99;
    void* second = patch.touch(1, &other);
    EXPECT_TRUE(first == second);
    EXPECT_EQ(value_of(patch, 1), 6);

    erased::put(patch, 1, 7);
    EXPECT_TRUE(patch.verified_at(0));

    // a tombstone survives touch
    erased::put(patch, 2, 20, true);
    *static_cast<int*>(patch.touch(2, &base)) = 21;
    EXPECT_TRUE(tombstone(patch, 2));
    EXPECT_EQ(value_of(patch, 2), 21);

    const int baseGlobal = 3;
    auto* global = static_cast<int*>(patch.touch_global(&baseGlobal));
    EXPECT_EQ(*global, 3);
    *global = 4;
    EXPECT_TRUE(patch.touch_global(&baseGlobal) == global);
    EXPECT_EQ(*global, 4);
}

void erased_patch_line_absorb()
{
    // a tombstone in the receiver beats a modification from the absorbed patch
    auto receiver = erased::int_patch();
    erased::put(receiver, 1, 10, true);
    erased::put(receiver, 2, 20);

    auto other = erased::int_patch();
    erased::put(other, 1, 11);
    erased::put(other, 2, 21);
    erased::put(other, 3, 30);
    erased::put(other, 4, 40, true);
    const int global = 9;
    other.set_global(&global);

    receiver.absorb(other);
    EXPECT_TRUE(tombstone(receiver, 1));
    EXPECT_EQ(value_of(receiver, 1), 11);
    EXPECT_FALSE(tombstone(receiver, 2));
    EXPECT_EQ(value_of(receiver, 2), 21);
    EXPECT_EQ(value_of(receiver, 3), 30);
    EXPECT_TRUE(tombstone(receiver, 4));
    EXPECT_EQ(*static_cast<const int*>(receiver.global()), 9);

    // a deletion absorbed over a modification
    auto modified = erased::int_patch();
    erased::put(modified, 5, 50);
    auto deleted = erased::int_patch();
    erased::put(deleted, 5, 51, true);
    modified.absorb(deleted);
    EXPECT_TRUE(tombstone(modified, 5));
}

void erased_patch_line_integrate_merge()
{
    fqsm::erased::Line state(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<int>());
    for (int i = 1; i <= 4; ++i)
        state.insert(static_cast<fqsm::RawId>(i), &i);

    auto patch = erased::int_patch();
    erased::put(patch, 1, 10);
    erased::put(patch, 2, 2, true);
    erased::put(patch, 5, 50);
    erased::put(patch, 6, 60, true);

    auto target = erased::int_patch();
    fqsm::erased::merge_into(state, target, patch);
    EXPECT_EQ(value_of(target, 1), 10);
    EXPECT_TRUE(tombstone(target, 2));
    EXPECT_EQ(value_of(target, 2), 2);
    EXPECT_EQ(value_of(target, 5), 50);
    EXPECT_FALSE(target.mention(6).found);

    fqsm::erased::integrate(state, patch);
    EXPECT_EQ(state.size(), std::size_t{4});
    EXPECT_EQ(*static_cast<const int*>(state.find(1)), 10);
    EXPECT_FALSE(state.contains(2));
    EXPECT_EQ(*static_cast<const int*>(state.find(5)), 50);
    EXPECT_FALSE(state.contains(6));

    const auto text = fqsm::erased::format_patch_line(patch, "Line");
    EXPECT_TRUE(text.starts_with("Line [#"));
    EXPECT_TRUE(fqsm::erased::format_patch_line(erased::int_patch(), "Line").empty());
}

void erased_patch_line_lifetime()
{
    using erased::Counted;
    using erased::Counts;

    Counts counts;
    {
        fqsm::erased::PatchLine patch(fqsm::erased::ops_of<Counted>(), fqsm::erased::ops_of<Counted>());
        Counted a(counts, "a");
        for (fqsm::RawId i = 0; i < 6; ++i)
            patch.modify(i, &a);
        patch.del(2, patch.mention(2).value);
        patch.discard(0);
        patch.set_global(&a);
        EXPECT_EQ(counts.alive(), 1 + 5 + 1);

        fqsm::erased::PatchLine copy(patch);
        copy.absorb(patch);
        EXPECT_EQ(counts.alive(), 1 + 12);
        patch.clear();
        EXPECT_EQ(counts.alive(), 1 + 6);
    }
    EXPECT_EQ(counts.alive(), 0);
}

} // namespace tests
