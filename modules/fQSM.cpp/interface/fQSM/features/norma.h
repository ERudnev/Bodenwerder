#pragma once

#include <fQSM/state/_forwards.h>

namespace fqsm::features {

    struct Norma {
        using Preview = state::world::Preview;
        using Patch = state::world::Patch;

        virtual ~Norma() = default;

        virtual void apply(const Preview& preUpdate, Patch& fixAccumulator) = 0;
    };
}