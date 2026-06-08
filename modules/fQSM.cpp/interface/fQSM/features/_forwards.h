#pragma once

#include <memory>
#include <vector>

// TODO: remove?
namespace fqsm::features {
    struct Codex;
    struct Reaction;

    using Reactions = std::vector<std::shared_ptr<Reaction>>;
}