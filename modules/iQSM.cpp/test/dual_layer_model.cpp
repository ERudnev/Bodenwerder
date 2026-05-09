#include "_common.h"

#include <iQSM/api/interface.h>

namespace {
    using namespace iqsm::interface;

    struct RuntimeEntity : Entity<RuntimeEntity> {
        struct Quantum { int value; };
    };

    struct AgentEntity : Agent<AgentEntity, RuntimeEntity> {
        struct Quantum { int value; };
    };

    struct LogicEntity : Entity<LogicEntity> {
        struct Quantum { int value; };
    };
}

namespace tests {

    // this test is TDD framework to envolve iQSM World of Two Layers: immutable (historical) and mutable (operational)
    void dual_layer_model()
    {
        using namespace iqsm::interface;
        //using Layer = policy::versioning;
        const auto schema = ask::schema::merge({
            ask::schema::aspect<LogicEntity, ask::schema::Layer::shared>(),
            ask::schema::aspect<AgentEntity, ask::schema::Layer::shared>(),
            ask::schema::aspect<RuntimeEntity, ask::schema::Layer::single>(),
        });
        
    }
}

