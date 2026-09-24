#pragma once

#include <cstdint>

namespace fqsm::erased {

    // Structural rules of the aspect categories, derived from descriptors when a schema is built.
    // listens: the slot whose changes arm the rule; subject and other: the two aspects it relates.
    enum class RuleKind : std::uint8_t {
        removeWithParent,                // other (host) removed -> delete subject (parasitic) with the same id
        deadParasiticKillsParent,        // subject (parasitic) removed -> delete other (host)
        newRequiresExistingParent,       // subject added -> other (host) must exist in the proposal
        newRequiresParentAppears,        // subject added -> other (host) must be added in the same patch
        parentAppearsRequiresComponent,  // other (host) added -> subject must exist in the proposal
        groupRemovalRemovesElements,     // subject (group) removed -> delete every element (other)
        elementRemovalUnhooks,           // other (element) removed -> erase it from every subject (group) value
    };

    struct Rule {
        RuleKind kind;
        std::uint32_t listens;
        std::uint32_t subject;
        std::uint32_t other;
    };
}
