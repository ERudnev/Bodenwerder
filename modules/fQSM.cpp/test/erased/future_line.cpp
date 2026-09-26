#include "_common.h"

#include <fQSM/erased/future_line.h>

#include "patch_stub.h"

namespace {
    const int& as_int(const void* value) { return *static_cast<const int*>(value); }
}

namespace tests {

void erased_future_line_writes()
{
    using namespace fqsm::erased;

    Line base(ops_of<int>(), ops_of<int>());
    for (int i = 1; i <= 3; ++i)
        base.insert(static_cast<fqsm::RawId>(i), &i);

    auto patch = erased::int_patch();
    FutureLine future(base, patch);

    // touch: first access copies the base value into an unverified patchlet, later accesses reuse it
    auto* touched = static_cast<int*>(future.get_modification_access(1));
    EXPECT_EQ(*touched, 1);
    EXPECT_FALSE(patch.verified_at(0));
    *touched = 10;
    EXPECT_TRUE(future.get_modification_access(1) == touched);
    EXPECT_EQ(as_int(future.find(1)), 10);
    EXPECT_EQ(as_int(base.find(1)), 1);
    EXPECT_TRUE(future.get_modification_access(4) == nullptr);

    int added = 40;
    future.put_add(4, &added);
    int changed = 20;
    future.put_modification(2, &changed);
    future.put_deletion(3);
    future.put_deletion(5);
    EXPECT_EQ(future.size(), std::size_t{3});
    const std::map<fqsm::RawId, int> expected{{1, 10}, {2, 20}, {4, 40}};
    EXPECT_TRUE(erased::collect(future) == expected);
    EXPECT_FALSE(patch.mention(5).found);
    EXPECT_EQ(as_int(patch.mention(3).value), 3);

    // a deleted id stays deleted through touch
    future.put_deletion(1);
    *static_cast<int*>(future.get_modification_access(1)) = 11;
    EXPECT_FALSE(future.contains(1));

    auto* global = static_cast<int*>(future.get_access_global());
    EXPECT_EQ(*global, 0);
    *global = 5;
    EXPECT_EQ(as_int(future.global()), 5);
    EXPECT_EQ(as_int(base.global()), 0);
    const int replaced = 6;
    future.put_global(&replaced);
    EXPECT_EQ(as_int(future.global()), 6);

    // a future over a future writes only into its own patch
    auto upper = erased::int_patch();
    FutureLine nested(future, upper);
    *static_cast<int*>(nested.get_modification_access(2)) = 21;
    EXPECT_EQ(as_int(nested.find(2)), 21);
    EXPECT_EQ(as_int(future.find(2)), 20);
    nested.put_deletion(4);
    EXPECT_FALSE(nested.contains(4));
    EXPECT_TRUE(future.contains(4));
    EXPECT_EQ(erased::steps(nested), std::size_t{1});
}

} // namespace tests
