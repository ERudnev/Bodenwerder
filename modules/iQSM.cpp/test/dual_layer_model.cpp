#include "_common.h"

#include <iQSM/api/interface.h>
#include "_model.h"


namespace tests {
    using namespace ::tests::model;
    // this test is TDD framework to envolve iQSM World of Two Layers: immutable (historical) and mutable (operational)
    void dual_layer_model()
    {
        using namespace iqsm::interface;
        const iqsm::Schema schema = ask::schema::merge({
            ask::schema::aspect<LogicEntity>(),
            ask::schema::aspect<ControllerEntity>(),
            ask::schema::aspect<RuntimeEntity>(),
        });

        const iqsm::World world = ask::world::create(schema);

        context::Branch master(world);  
        
        const auto entityId = ask::item::create<LogicEntity>(master, {1});
    }
}

