#pragma once

#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/renderer/types.q1.h>

#include "views/blueprints/geometry.h"

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

    struct QuarkRef {
        QuarkActor::Kind kind;
        std::size_t cell;
        std::size_t index;
    };

    struct Store {
        std::vector<rmmr::renderer::Integer32> aliases;
        Blueprint clipboard; // cells = paste buffer; cell.pose = preview placement
        std::vector<QuarkRef> pendingRestore;
        Focus focus;
    };

    auto sameIndex3(const index3& a, const index3& b) -> bool;
    auto cellOccupied(const Blueprint& data, const index3& pos) -> bool;
    auto clipboardEmpty(const Store&) -> bool;
    auto canPaste(const Blueprint& target, const Blueprint& clipboard) -> bool;

    void clear(Store&);
    void selectAll(Reading, Store&, const std::vector<QuarkActor>& actors);
    void resetClipboard(Store&);
    void toggleFocus(Store&);
    void rematchAfterSync(Reading, Store&, const std::vector<QuarkActor>& actors);

    void expand(Reading, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& actors);
    void copyToClipboard(Reading, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& actors);

    auto eraseSelected(Writing, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& actors) -> bool;
    auto rotateSelected(Writing, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& actors, mech::space::orient::Semiaxis) -> bool;
    auto moveSelected(Writing, Store&, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& actors, index3 step) -> bool;

    auto rotateClipboard(Store&, mech::space::orient::Semiaxis) -> bool;
    auto moveClipboard(Store&, index3 step) -> bool;
    auto pasteClipboard(Writing, Store&, mech::Blueprint::Id hovered) -> bool;

    void handlePointer(Reading, Store&, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& actors, rmmr::renderer::Integer32 under);

    auto handleHotkeys(Writing, Store&, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;
    auto handleClipboardHotkeys(Store&) -> bool;
    auto handleClipboardChords(Writing, Store&, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;

    auto drawPanel(Writing, Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& actors) -> bool;
    auto drawClipboardPanel(Writing, Store&, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<mech::Blueprint::Id> hovered) -> bool;

} // namespace eltanin::views::blueprints::selection
