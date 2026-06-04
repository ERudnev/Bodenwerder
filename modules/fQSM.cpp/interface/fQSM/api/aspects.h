#pragma once

#include <fQSM/identifier.h>
#include <fQSM/features/interface.h>

namespace fqsm::detail::aspects {
    template<typename Meta>
    struct Any {
        Any() = delete;
        using Codex = fqsm::service::internal::Codex;
    };
}

namespace fqsm::aspects {

    // this classes are base of final Aspects (metaclasses)
    template<typename Meta>
    struct Entity : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
        using DefaultService = service::Group<Meta>;
    };

    template<typename Meta, typename WorkerType>
    struct Controller : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
        using DefaultService = service::Group<Meta>;
        using WorkerAspect = WorkerType;
    };

    template<typename Meta, typename HostType>
    struct Attribute : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
        using DefaultService = service::Group<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Component : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
        using DefaultService = service::Group<Meta>;
    };
}

