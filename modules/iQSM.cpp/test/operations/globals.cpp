#include "../_common.h"

#include <iQSM/operations/integration.h>

#include <Etalon/aspects.q1.h>

namespace tests {
    void globals() {
        using namespace iqsm;
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        const World before = ops::world::create_no_resources(ops::schema::assemble<Tag>());

        EXPECT_EQ(ops::global::get<Tag>(before)->modulus, integer{2});

        auto tx = repo::Sequence{before};
        ops::global::modifier<Tag>(tx)->modulus = integer{1};
        ops::global::modifier<Tag>(tx)->modulus = integer{2};

        const World validated = operations::validate_smart(before, operations::integrate(before, tx.push()));
        EXPECT_EQ(ops::global::get<Tag>(validated)->modulus, integer{2});
    }
}
