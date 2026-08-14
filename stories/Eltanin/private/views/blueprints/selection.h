#pragma once

#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/renderer/types.q1.h>

#include "views/blueprints/geometry.h"
#include "views/blueprints/history.h"

#include "mech/semantics/space.h"

#include <fQSM/api/interface.h>

#include <imgui.h>

namespace eltanin::views::blueprints::selection {

    using namespace fqsm::api;
    using geometry::QuarkActor;
    using geometry::MountActor;

    enum class Focus {
        selection,
        clipboard,
    };

    struct QuarkRef {
        QuarkActor::Kind kind;
        std::size_t cell;
        std::size_t index;
    };

    struct MountRef {
        std::size_t index; // into Blueprint::mounts
    };

    struct Store {
        std::vector<rmmr::renderer::Integer32> aliases;
        Blueprint clipboard; // cells + mounts = paste buffer
        std::vector<QuarkRef> pendingRestore;
        std::vector<MountRef> pendingMountRestore;
        Focus focus;
    };

    auto sameIndex3(const index3& a, const index3& b) -> bool;
    auto cellOccupied(const Blueprint& data, const index3& pos) -> bool;
    auto clipboardEmpty(const Store&) -> bool;
    auto canPaste(const Blueprint& target, const Blueprint& clipboard) -> bool;

    void clear(Store&);
    void selectAll(Reading, Store&, const std::vector<QuarkActor>&, const std::vector<MountActor>&);
    void resetClipboard(Store&);
    void toggleFocus(Store&);
    void rematchAfterSync(Reading, Store&, const std::vector<QuarkActor>&, const std::vector<MountActor>&);

    void expand(Reading, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&);
    void copyToClipboard(Reading, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&);

    auto eraseSelected(Writing, Store&, history::Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&) -> bool;
    auto rotateSelected(Writing, Store&, history::Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&, mech::space::orient::Semiaxis) -> bool;
    auto moveSelected(Writing, Store&, history::Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&, index3 step) -> bool;

    auto rotateClipboard(Store&, mech::space::orient::Semiaxis) -> bool;
    auto moveClipboard(Store&, index3 step) -> bool;
    auto pasteClipboard(Writing, Store&, history::Store&, mech::Blueprint::Id hovered) -> bool;

    // Empty if under is not a placed-mount actor alias.
    auto hitMount(Reading, const std::vector<MountActor>&, rmmr::renderer::Integer32 under) -> base::maybe<MountActor>;

    void handlePointer(Reading, Store&, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&, rmmr::renderer::Integer32 under);

    auto handleHotkeys(Writing, Store&, history::Store&, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&) -> bool;
    auto handleClipboardHotkeys(Store&) -> bool;
    auto handleClipboardChords(Writing, Store&, history::Store&, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&) -> bool;

    auto drawPanel(Writing, Store&, history::Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>&, const std::vector<MountActor>&) -> bool;
    auto drawClipboardPanel(Writing, Store&, history::Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<mech::Blueprint::Id> hovered) -> bool;

} // namespace eltanin::views::blueprints::selection
