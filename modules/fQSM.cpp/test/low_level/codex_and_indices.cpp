#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world/data.h>

namespace tests {

void codex_and_indices()
{
    using namespace ::tests::model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
        ask::schema::aspect<SomeComponent>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);


    const auto id = ask::item::create<SomeEntity>(main, {4});
    EXPECT_EQ(ask::item::get<SomeComponent>(main, id)->name, "second");
}

} // namespace tests
