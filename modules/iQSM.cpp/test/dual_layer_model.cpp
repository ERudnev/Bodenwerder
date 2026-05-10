#include "_common.h"

#include <iQSM/api/interface.h>
#include "_model.h"


namespace tests {
    using namespace ::tests::model;
    // this test is TDD framework to envolve iQSM World of Two Layers: immutable (historical) and mutable (operational)
    void dual_layer_model()
    {
        using namespace iqsm::interface;
        //using Layer = policy::versioning;
        const iqsm::Schema schema = ask::schema::merge({
            ask::schema::aspect<LogicEntity, ask::schema::Layer::shared>(),
            ask::schema::aspect<AgentEntity, ask::schema::Layer::shared>(),
            ask::schema::aspect<RuntimeEntity, ask::schema::Layer::single>(),
        });

        const iqsm::World world = ask::world::create(schema);

        context::Branch branch(world);
        
    }
}

