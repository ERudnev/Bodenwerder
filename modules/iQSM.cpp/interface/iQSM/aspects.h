#pragma once

#include <iQSM/identifier.h>
#include <iQSM/service/interface.h>


namespace iqsm::detail::aspects {
    template<typename Meta>
    struct Any {
        Any() = delete;
    };
}

namespace iqsm::aspects {

    // all local types are re-defined to get rid of "typename Meta::Id", "typename Meta::OwnManipulators" e.t.c
    template<typename Meta>
    struct Entity : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
        using OwnManipulators = ::iqsm::service::Group<Meta>;
    };

    template<typename Meta, typename WorkerType>
    struct Controller : detail::aspects::Any<Meta> {
        using Id = Identifier<Meta>;
        using OwnManipulators = service::Group<Meta>;
        using WorkerAspect = WorkerType;
    };

    template<typename Meta, typename HostType>
    struct Attribute : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
        using OwnManipulators = service::Group<Meta>;
    };

    template<typename Meta, typename HostType>
    struct Component : detail::aspects::Any<Meta> {
        using Id = typename HostType::Id;
        using HostAspect = HostType;
        using OwnManipulators = service::Group<Meta>;
    };
}

