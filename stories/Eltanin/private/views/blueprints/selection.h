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

    struct Store {
        std::vector<rmmr::renderer::Integer32> aliases;
        mech::Blueprint clipboard; // paste buffer ("буфер"); not a disk asset
        std::vector<std::pair<QuarkActor::Kind, std::size_t>> pendingRestore; // rematch aliases after syncVisuals
    };

    auto sameIndex3(const index3& a, const index3& b) -> bool;
    auto cellOccupied(const mech::Blueprint& data, const index3& pos) -> bool;

    void clear(Store&);
    void selectAll(Reading, Store&, const std::vector<QuarkActor>& actors);
    void resetClipboard(Store&);
    void rematchAfterSync(Reading, Store&, const std::vector<QuarkActor>& actors);

    void expand(Reading, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors);
    void copyToClipboard(Reading, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors);

    // Mutate blueprint quarks for selected cell families. Caller persists + syncs visuals when true.
    auto eraseSelected(Writing, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors) -> bool;
    auto rotateSelected(Writing, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, mech::space::orient::Semiaxis) -> bool;
    auto moveSelected(Writing, Store&, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, index3 step) -> bool;

    // LMB/RMB ± Shift pick/deselect. under == 0 + LMB clears.
    void handlePointer(Reading, Store&, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors, rmmr::renderer::Integer32 under);

    // Del / WASD+QE / Ctrl+WASD+QE. Returns true if blueprint content changed.
    auto handleHotkeys(Writing, Store&, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;

    // Selection ImGui window next to Blueprints: no close, collapsible; empty → select all; list grouped by cell.
    void drawPanel(Reading, Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors);

} // namespace eltanin::views::blueprints::selection
