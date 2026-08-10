#pragma once

#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/resources/blueprint.q1.h>
#include <rmmr/renderer/types.q1.h>

#include "views/blueprints/geometry.h"

#include "mech/blueprint.h"
#include "mech/semantics/space.h"

#include <fQSM/api/interface.h>

#include <imgui.h>

namespace eltanin::views::blueprints::selection {

    using namespace fqsm::api;
    using geometry::QuarkActor;

    enum class Focus {
        selection,
        clipboard,
    };

    struct Store {
        std::vector<rmmr::renderer::Integer32> aliases;
        mech::Blueprint clipboard; // editor-global paste buffer (not per-blueprint); poses = preview placement
        std::vector<std::pair<QuarkActor::Kind, std::size_t>> pendingRestore; // rematch aliases after syncVisuals
        Focus focus;
    };

    auto sameIndex3(const index3& a, const index3& b) -> bool;
    auto cellOccupied(const mech::Blueprint& data, const index3& pos) -> bool;
    auto clipboardEmpty(const Store&) -> bool;
    auto canPaste(const mech::Blueprint& target, const mech::Blueprint& clipboard) -> bool;

    void clear(Store&);
    void selectAll(Reading, Store&, const std::vector<QuarkActor>& actors);
    void resetClipboard(Store&);
    void toggleFocus(Store&);
    void rematchAfterSync(Reading, Store&, const std::vector<QuarkActor>& actors);

    void expand(Reading, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors);
    void copyToClipboard(Reading, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors);

    // Mutate blueprint quarks for selected cell families. Caller persists + syncs visuals when true.
    auto eraseSelected(Writing, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors) -> bool;
    auto rotateSelected(Writing, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, mech::space::orient::Semiaxis) -> bool;
    auto moveSelected(Writing, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, index3 step) -> bool;

    // Mutate clipboard poses (preview). Move is free; occupancy is enforced only on paste.
    auto rotateClipboard(Store&, mech::space::orient::Semiaxis) -> bool;
    auto moveClipboard(Store&, index3 step) -> bool;
    auto pasteClipboard(Writing, Store&, resource::blueprint::Asset::Id hovered) -> bool;

    // LMB/RMB ± Shift pick/deselect. under == 0 + LMB clears.
    void handlePointer(Reading, Store&, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors, rmmr::renderer::Integer32 under);

    // Del / WASD+QE move / Shift+WASD+QE rotate for selection focus. Returns true if blueprint content changed.
    auto handleHotkeys(Writing, Store&, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;
    // WASD+QE move / Shift+WASD+QE rotate for clipboard focus. Returns true if clipboard poses changed.
    auto handleClipboardHotkeys(Store&) -> bool;
    // Ctrl+C copy selection → clipboard; Ctrl+V paste when allowed. No Ctrl+X. Returns true if paste mutated the asset.
    auto handleClipboardChords(Writing, Store&, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;

    // Selection ImGui window next to Blueprints: no close, collapsible; empty → select all; list grouped by cell.
    // Returns true if delete mutated the hovered asset.
    auto drawPanel(Writing, Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;

    // Minimal clipboard window: counts, can/blocked, focus toggle, paste, clear. Returns true if paste mutated the asset.
    auto drawClipboardPanel(Writing, Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered) -> bool;

} // namespace eltanin::views::blueprints::selection
