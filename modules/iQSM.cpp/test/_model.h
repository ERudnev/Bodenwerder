#pragma once

#include <iQSM/api/interface.h>

// this classes are kinda generated from Q1
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


// this is prototype of type-registration mechanism
namespace iqsm_fork {

    template<::iqsm::meta::Aspect Meta, iqsm::state::policy::versioning VersioningType>
    struct Register : Meta {
        struct _Traits {
            //static constexpr iqsm::state::policy::versioning versioning = Versioning;
            using Versioning = std::integral_constant<iqsm::state::policy::versioning, VersioningType>;
        };
    };
}

// this is "facade" domain
namespace tests::model {
    using namespace iqsm_fork;

    using RuntimeEntity = Register<tests::generated_domain::RuntimeEntity, iqsm::state::policy::versioning::single>;
    using AgentEntity = Register<tests::generated_domain::AgentEntity, iqsm::state::policy::versioning::shared>;
    using LogicEntity = Register<tests::generated_domain::LogicEntity, iqsm::state::policy::versioning::shared>;
}

