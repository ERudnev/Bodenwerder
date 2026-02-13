#pragma once

#include <iQSM/q1/_gateway.h>

#include <string>
#include <vector>

namespace Q1CORE::Example::System {
    using namespace iqsm::dsl_gateway;

    struct MolecularStatistics : Xion<MolecularStatistics>, Require<> {
        struct Entry {
            string name;
            integer count;
        };

        struct Quantum {
            std::vector<Entry> entries;
        };

        inline static const Structural invariants{{{
        }}};
    };
}


