#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <algorithm>

#include <fQSM/state/table.h>

namespace {
    template<typename T>
    auto sorted(std::vector<T> values) -> std::vector<T>
    {
        std::sort(values.begin(), values.end());
        return values;
    }
}

namespace tests {

void relations_table()
{
    using namespace ::tests::model;

    fqsm::state::relations::Table<SomeEntity, SomeComponent> table;

    const fqsm::Id<SomeEntity> e1{ 1 };
    const fqsm::Id<SomeEntity> e2{ 2 };
    const fqsm::Id<SomeComponent> c10{ 10 };
    const fqsm::Id<SomeComponent> c11{ 11 };
    const fqsm::Id<SomeComponent> c12{ 12 };

    EXPECT_TRUE(table.insert_relation(e1, c10));
    EXPECT_TRUE(table.insert_relation(e1, c11));
    EXPECT_TRUE(table.insert_relation(e2, c10));
    EXPECT_TRUE(table.insert_relation(e2, c12));
    EXPECT_FALSE(table.insert_relation(e1, c10));

    EXPECT_TRUE(sorted(table.followers(e1)) == (std::vector<fqsm::Id<SomeComponent>>{ c10, c11 }));
    EXPECT_TRUE(sorted(table.followers(e2)) == (std::vector<fqsm::Id<SomeComponent>>{ c10, c12 }));
    EXPECT_TRUE(sorted(table.origins(c10)) == (std::vector<fqsm::Id<SomeEntity>>{ e1, e2 }));
    EXPECT_TRUE(table.origins(c11) == (std::vector<fqsm::Id<SomeEntity>>{ e1 }));

    EXPECT_TRUE(table.erase_relation(e1, c10));
    EXPECT_FALSE(table.erase_relation(e1, c10));
    EXPECT_TRUE(table.followers(e1) == (std::vector<fqsm::Id<SomeComponent>>{ c11 }));
    EXPECT_TRUE(table.origins(c10) == (std::vector<fqsm::Id<SomeEntity>>{ e2 }));

    table.erase_follower(c12);
    EXPECT_TRUE(table.followers(e2) == (std::vector<fqsm::Id<SomeComponent>>{ c10 }));
    EXPECT_TRUE(table.origins(c12).empty());

    table.erase_origin(e1);
    EXPECT_TRUE(table.followers(e1).empty());
    EXPECT_TRUE(table.origins(c11).empty());

    fqsm::state::relations::Table<SomeEntity, SomeEntity> self;
    const fqsm::Id<SomeEntity> e3{ 3 };

    EXPECT_TRUE(self.insert_relation(e1, e2));
    EXPECT_TRUE(self.insert_relation(e1, e3));
    EXPECT_TRUE(self.insert_relation(e2, e1));

    EXPECT_TRUE(sorted(self.followers(e1)) == (std::vector<fqsm::Id<SomeEntity>>{ e2, e3 }));
    EXPECT_TRUE(sorted(self.origins(e1)) == (std::vector<fqsm::Id<SomeEntity>>{ e2 }));

    self.erase_origin(e1);
    EXPECT_TRUE(self.followers(e1).empty());
    EXPECT_TRUE(self.origins(e2).empty());
    EXPECT_TRUE(self.origins(e3).empty());
}

} // namespace tests
