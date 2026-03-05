#pragma once

#include <iQSM/identifier.h>

namespace iqsm::aspects {

    template<typename Meta>
    struct Base {
        // disallow constructing any meta-type (Particle) as objects:
        Base() = delete;
    };

    template<typename Meta>
    struct Entity : Base<Meta> {
        using Id = Identifier<Meta>;
    };

    template<typename Meta, typename Parent>
    struct Attribute : Base<Meta> {
        using Id = typename Parent::Id;
        using ParentAspect = Parent;
    };

    template<typename Meta, typename Parent>
    struct Component : Base<Meta> {
        using Id = typename Parent::Id;
        using ParentAspect = Parent;
    };

    template<typename Meta>
    struct Resource : Base<Meta> {
        using Id = Identifier<Meta>;
    };

}