#include "_common.h"

#include <Etalon/aspects.q1.h>

namespace tests {
    
    void model_is_compileable() {
        using namespace iqsm::dsl_gateway;
        using namespace Q1CORE::Etalon;

        const auto schema = ops::schema::assemble<SampleEntity, Tag, SampleComponent, SampleAttribute>();
        const auto empty = ops::world::create(schema);
        (void)empty;
        EXPECT_TRUE(true);
    }
}

