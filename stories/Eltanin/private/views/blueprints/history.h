#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>

#include <base/maybe.h>
#include <eltanin/mech/blueprint.q1.h>

#include <fQSM/api/interface.h>

#include <imgui.h>

namespace eltanin::views::blueprints::history {

    using namespace fqsm::api;
    using Blueprint = mech::Blueprint::Quantum;

    // Poor-man's undo: full document copies only. No command pattern.
    inline constexpr std::size_t depth = 16;

    struct Step {
        std::string label;
        Blueprint document;
    };

    struct Store {
        base::maybe<mech::Blueprint::Id> bound;
        std::deque<Step> undo;
        std::deque<Step> redo;
    };

    void clear(Store&);
    void bind(Store&, base::maybe<mech::Blueprint::Id>);

    // Push snapshot of document BEFORE an edit. Clears redo. Drops oldest past depth.
    void record(Store&, mech::Blueprint::Id, std::string label, const Blueprint& before);

    auto canUndo(const Store&) -> bool;
    auto canRedo(const Store&) -> bool;

    // Restore previous snapshot; current goes to redo. Caller syncs actors after true.
    auto undo(Writing, Store&, mech::Blueprint::Id) -> bool;
    auto redo(Writing, Store&, mech::Blueprint::Id) -> bool;

    enum class UiAction : std::uint8_t { none, undo, redo };
    auto drawWindow(Store&) -> UiAction;

}
