#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <fQSM/api/interface.h>

namespace tests {

void globals()
{
    using namespace ::tests::model;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<SomeEntity>(),
        ask::schema::aspect<SomeComponent>(),
    });

    context::Realm main(schema);

    EXPECT_EQ(ask::global::get<SomeEntity>(main).modulus, 2);

    {
        auto tx = ask::global::update<SomeEntity>(main);
        tx->modulus = 7;
    }

    EXPECT_EQ(ask::global::get<SomeEntity>(main).modulus, 7);
}

} // namespace tests
