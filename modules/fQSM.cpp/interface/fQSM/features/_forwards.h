#pragma once

#include <memory>
#include <vector>

namespace fqsm::features::reactions {
    struct Abstract;

    template<typename ActionFunction>
    struct Functional;

    // each entry in model::complex::Patch::Result::critical is one failure
    using FailedCount = size_t;
}

namespace fqsm::features {
    // TODO: hide this word from this namespace
    struct Behavior;

    using Reactions = std::vector<std::shared_ptr<reactions::Abstract>>;
}