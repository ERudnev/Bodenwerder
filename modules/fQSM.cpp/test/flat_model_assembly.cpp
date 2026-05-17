#include "_common.h"

#include "_model.h"
#include <fQSM/api/interface.h>


namespace tests {
    using namespace ::tests::model;
    // this test is TDD framework to envolve fQSM World
    void flat_model_assembly()
    {
        using namespace fqsm::api;
        const Schema schema = ask::schema::merge({
            ask::schema::aspect<SomeEntity>(),
            ask::schema::aspect<SomeComponent>(),
        });
       
    }
}

