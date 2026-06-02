#include "_common.h"
#include "_model.h"

#include <fQSM/api/interface.h>
#include <fQSM/state/world.h>

namespace tests {

void globals()
{
    using namespace ::tests::model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
        ask::schema::aspect<SomeComponent>(),
    });

    fqsm::state::world::Data world(schema);
    context::Realm main(world);

    EXPECT_EQ(ask::global::get<SomeEntity>(main).modulus, 2);

    {
        auto tx = ask::global::update<SomeEntity>(main);
        tx->modulus = 7;
    }

    EXPECT_EQ(ask::global::get<SomeEntity>(main).modulus, 7);
}

} // namespace tests
