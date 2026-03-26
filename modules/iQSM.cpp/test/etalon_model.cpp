#include "_common.h"

#include "etalon/model.h"

namespace tests {
    
    void etalon_model() {
        using namespace iqsm::dsl_gateway;
        using namespace iqsm_internal_model;

        const auto empty = ops::world::create(ops::schema::assemble<Essentials, Optionals>());

        repo::Branch master{empty};
        EXPECT_EQ(static_cast<decltype(empty)>(master), empty);
    }
}

