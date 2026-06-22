#pragma once

#include <memory>
#include <vector>

namespace fqsm::features::reactions {
    struct Abstract;

    template<typename ActionFunction>
    struct Functional;
}

namespace fqsm::features {
    struct Codex;
    using Reactions = std::vector<std::shared_ptr<reactions::Abstract>>;
}