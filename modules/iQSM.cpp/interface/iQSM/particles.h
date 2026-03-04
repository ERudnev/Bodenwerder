#pragma once

#include <iQSM/identifier.h>

namespace iqsm {

    namespace particles {
        template<typename Meta>
        struct Base {
            // disallow constructing any meta-type (Particle) as objects:
            Base() = delete;
        };

        template<typename Meta>
        struct Xion : Base<Meta> {
            using Id = Identifier<Meta>;
        };

        template<typename Meta, typename Parent>
        struct Quark : Base<Meta> {
            using Id = typename Parent::Id;
        };

        template<typename Meta>
        struct Resource : Base<Meta> {
            using Id = Identifier<Meta>;
        };
    }
}