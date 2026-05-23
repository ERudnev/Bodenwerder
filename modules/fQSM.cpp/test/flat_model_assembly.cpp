#include "_common.h"

#include "_model.h"
#include <fQSM/api/interface.h>

// temp
#include <fQSM/state/world.h>


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

        //fqsm::World world = ask::world::create(schema);
        fqsm::state::world::Data world(schema);
        context::Realm realm(world);
       
    }
}

