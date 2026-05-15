#pragma once

#include <iQSM/api/interface.h>

// KQM: &iQSM::Archetype, &iQSM::Aspect, &iQSM::archetype_to_aspect_transition, &iQSM::runtime_is_system_projection
namespace tests::generated_domain {
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

// this is "facade" domain
namespace tests::model {
    using namespace iqsm::interface;
    using RuntimeEntity = Register<generated_domain::RuntimeEntity, Layer::single>;
    using AgentEntity = Register<generated_domain::AgentEntity, Layer::shared>;
    using LogicEntity = Register<generated_domain::LogicEntity, Layer::shared>;
}

