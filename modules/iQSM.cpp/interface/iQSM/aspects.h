#pragma once

#include <iQSM/identifier.h>
#include <iQSM/service/interface.h>


namespace iqsm::detail::aspects {
    template<typename Meta>
    struct Base {
        Base() = delete;
    };
}

namespace iqsm::aspects {
    template<typename Meta>
    struct Entity : detail::aspects::Base<Meta> {
        using Id = Identifier<Meta>;
        using OwnManipulators = ::iqsm::service::Group<Meta>;
    };

    template<typename Meta, typename Parent>
    struct Attribute : detail::aspects::Base<Meta> {
        using Id = typename Parent::Id;
        using ParentAspect = Parent;
        using OwnManipulators = service::Group<Meta>;
    };

    template<typename Meta, typename Parent>
    struct Component : detail::aspects::Base<Meta> {
        using Id = typename Parent::Id;
        using ParentAspect = Parent;
        using OwnManipulators = service::Group<Meta>;
    };

    template<typename Meta, typename RuntimeType>
    struct Agent : detail::aspects::Base<Meta> {
        using Id = Identifier<Meta>;
        using OwnManipulators = service::Group<Meta>;
        // TODO: redesign this...
        using Runtime = RuntimeType;
    };
}

