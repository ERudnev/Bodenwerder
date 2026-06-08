#pragma once

#include <memory>
#include <vector>

// TODO: remove?
namespace fqsm::features {
    struct Codex;
    struct Norma;

    using Normas = std::vector<std::shared_ptr<Norma>>;
}