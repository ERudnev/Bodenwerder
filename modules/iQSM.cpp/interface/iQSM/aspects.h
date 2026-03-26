#pragma once

#include <iQSM/identifier.h>
#include <iQSM/operations/interface.h>


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
        using OwnTypeOperations = ::iqsm::detail::operations::Group<Meta>;
    };

    template<typename Meta, typename Parent>
    struct Attribute : detail::aspects::Base<Meta> {
        using Id = typename Parent::Id;
        using ParentAspect = Parent;
        using OwnTypeOperations = ::iqsm::detail::operations::Group<Meta>;
    };

    template<typename Meta, typename Parent>
    struct Component : detail::aspects::Base<Meta> {
        using Id = typename Parent::Id;
        using ParentAspect = Parent;
        using OwnTypeOperations = ::iqsm::detail::operations::Group<Meta>;
    };

    template<typename Meta>
    struct Resource : detail::aspects::Base<Meta> {
        using Id = Identifier<Meta>;
        using OwnTypeOperations = ::iqsm::detail::operations::Group<Meta>;
    };
}

