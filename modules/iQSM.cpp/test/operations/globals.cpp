#include "../_common.h"

#include <iQSM/operations/integration.h>

#include <Etalon/aspects.q1.h>

namespace tests {
    void globals() {
        using namespace iqsm;
        using namespace iqsm::dsl_gateway;
        using namespace Q1_iQSM::Etalon;

        const World before = ops::world::create_no_resources(ops::schema::assemble<Tag>());

        EXPECT_EQ(ops::global::get<Tag>(before)->modulus, integer{2});

        iqsm::repo::Sequence transaction{before};
        ops::global::modifier<Tag>(transaction)->modulus = integer{1};
        ops::global::modifier<Tag>(transaction)->modulus = integer{2};


        EXPECT_EQ(ops::global::get<Tag>(transaction)->modulus, integer{2});
    }
}
