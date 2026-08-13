#include "views/blueprints/selection.h"

#include "mech/semantics/quarks.h"
#include "mech/semantics/subframe.h"

#include <eltanin/resources/blueprint.q1.h>
#include <fQSM/identifier.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace eltanin::views::blueprints::selection {

    using namespace rmmr;

    namespace {

        struct CellPosLess {
            auto operator()(const index3& a, const index3& b) const -> bool {
                return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
            }
        };

        auto addIndex3(const index3& a, const index3& b) -> index3 {
            return index3{.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
        }

        auto cellsPivot(const std::set<index3, CellPosLess>& cells) -> index3 {
            auto min = *cells.begin();
            auto max = min;
            for (const auto& cell : cells) {
                min.x = std::min(min.x, cell.x);
                min.y = std::min(min.y, cell.y);
                min.z = std::min(min.z, cell.z);
                max.x = std::max(max.x, cell.x);
                max.y = std::max(max.y, cell.y);
                max.z = std::max(max.z, cell.z);
            }
            return index3{.x = (min.x + max.x) / 2, .y = (min.y + max.y) / 2, .z = (min.z + max.z) / 2};
        }

        auto rotateCellPos(const index3& pos, const index3& pivot, mech::space::orient::Semiaxis axis) -> index3 {
            const auto delta = mech::space::orient::turn(axis)[0];
            const auto& rotation = mech::space::orient::matrix[static_cast<std::size_t>(delta)];
            const auto rel = rotation * mech::space::ivec3{pos.x - pivot.x, pos.y - pivot.y, pos.z - pivot.z};
            return index3{.x = rel.x + pivot.x, .y = rel.y + pivot.y, .z = rel.z + pivot.z};
        }

        auto remapCells(const std::set<index3, CellPosLess>& cells, mech::space::orient::Semiaxis axis) -> std::map<index3, index3, CellPosLess> {
            std::map<index3, index3, CellPosLess> remap;
            const auto pivot = cellsPivot(cells);
            for (const auto& from : cells)
                remap.emplace(from, rotateCellPos(from, pivot, axis));
            return remap;
        }

        auto findActorByAlias(Reading context, const std::vector<QuarkActor>& actors, renderer::Integer32 alias) -> base::maybe<QuarkActor> {
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias == alias)
                    return actor;
            }
            return {};
        }

        auto actorByAliasIndex(Reading context, const std::vector<QuarkActor>& actors) -> std::unordered_map<renderer::Integer32, const QuarkActor*> {
            std::unordered_map<renderer::Integer32, const QuarkActor*> out;
            out.reserve(actors.size());
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                out.emplace(with<scene::actor::Identified>::get(context, actor.id).scenicAlias, &actor);
            }
            return out;
        }

        void eraseDescending(auto& vec, const std::set<std::size_t>& indices) {
            for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
                if (*it < vec.size())
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(*it));
            }
        }

        auto selectionRowLabel(const mech::Blueprint* data, const base::maybe<QuarkActor>& actor, renderer::Integer32 alias) -> std::string {
            const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
            if (not actor or not data or actor->cell >= data->cells.size())
                return std::format("?  #{}", hash);
            const auto& cell = data->cells[actor->cell];
            const auto& pos = cell.pose.pos;
            switch (actor->kind) {
                case QuarkActor::Kind::knot: {
                    if (actor->index >= cell.frame.corners.size())
                        return std::format("knot[{}:{}]  #{}", actor->cell, actor->index, hash);
                    const auto& knot = cell.frame.corners[actor->index];
                    return std::format("knot[{}:{}] {}  cell[{},{},{}] localOri={}  #{}", actor->cell, actor->index, mech::skeleton::cornerSpecs.at(knot.kind).code, pos.x, pos.y, pos.z, knot.ori, hash);
                }
                case QuarkActor::Kind::halfChord: {
                    if (actor->index >= cell.frame.halfribs.size())
                        return std::format("halfChord[{}:{}]  #{}", actor->cell, actor->index, hash);
                    const auto& halfChord = cell.frame.halfribs[actor->index];
                    const char pole = halfChord.pole == mech::skeleton::Halfrib::Pole::starts ? 's' : 'e';
                    return std::format("halfChord[{}:{}] {}:{}  cell[{},{},{}] localOri={}  #{}", actor->cell, actor->index, mech::skeleton::halfribSpecs.at(halfChord.kind).code, pole, pos.x, pos.y, pos.z, halfChord.ori, hash);
                }
                case QuarkActor::Kind::wall: {
                    if (actor->index >= cell.hull.membranes.size())
                        return std::format("wall[{}:{}]  #{}", actor->cell, actor->index, hash);
                    const auto& wall = cell.hull.membranes[actor->index];
                    return std::format("wall[{}:{}] {}  cell[{},{},{}] localOri={}  #{}", actor->cell, actor->index, mech::skeleton::membraneSpecs.at(wall.kind).code, pos.x, pos.y, pos.z, wall.ori, hash);
                }
            }
            return std::format("?  #{}", hash);
        }

        auto selectionRefs(Reading context, const std::vector<QuarkActor>& actors, const std::vector<renderer::Integer32>& aliases) -> std::vector<QuarkRef> {
            std::vector<QuarkRef> refs;
            for (const auto alias : aliases) {
                if (const auto actor = findActorByAlias(context, actors, alias))
                    refs.push_back(QuarkRef{.kind = actor->kind, .cell = actor->cell, .index = actor->index});
            }
            return refs;
        }

        void restoreAliases(Reading context, const std::vector<QuarkActor>& actors, const std::vector<QuarkRef>& refs, std::vector<renderer::Integer32>& aliases) {
            aliases.clear();
            for (const auto& ref : refs) {
                for (const auto& actor : actors) {
                    if (actor.kind != ref.kind or actor.cell != ref.cell or actor.index != ref.index)
                        continue;
                    if (not with<scene::actor::Identified>::exists(context, actor.id))
                        break;
                    aliases.push_back(with<scene::actor::Identified>::get(context, actor.id).scenicAlias);
                    break;
                }
            }
        }

        auto actorAlias(Reading context, const QuarkActor& actor) -> base::maybe<renderer::Integer32> {
            if (not with<scene::actor::Identified>::exists(context, actor.id))
                return {};
            return with<scene::actor::Identified>::get(context, actor.id).scenicAlias;
        }

        void addAlias(std::vector<renderer::Integer32>& aliases, renderer::Integer32 alias) {
            if (alias == renderer::Integer32{0})
                return;
            if (std::find(aliases.begin(), aliases.end(), alias) == aliases.end())
                aliases.push_back(alias);
        }

        void removeAlias(std::vector<renderer::Integer32>& aliases, renderer::Integer32 alias) {
            aliases.erase(std::remove(aliases.begin(), aliases.end(), alias), aliases.end());
        }

        auto familyAliases(Reading context, const std::vector<QuarkActor>& actors, const QuarkActor& hit) -> std::vector<renderer::Integer32> {
            std::vector<renderer::Integer32> out;
            for (const auto& actor : actors) {
                if (actor.cell != hit.cell)
                    continue;
                if (const auto alias = actorAlias(context, actor))
                    out.push_back(*alias);
            }
            return out;
        }

        auto selectedCellIndices(Reading context, const Store& store, const std::vector<QuarkActor>& actors) -> std::set<std::size_t> {
            std::set<std::size_t> cells;
            for (const auto alias : store.aliases) {
                if (const auto actor = findActorByAlias(context, actors, alias))
                    cells.insert(actor->cell);
            }
            return cells;
        }

        auto cellEmpty(const mech::Blueprint::Cell& cell) -> bool {
            return cell.frame.corners.empty() and cell.frame.halfribs.empty() and cell.hull.membranes.empty();
        }

        auto rotateCellPose(mech::space::cell::Pose& pose, mech::space::orient::Semiaxis axis, const std::map<index3, index3, CellPosLess>& remap) -> void {
            const auto delta = mech::space::orient::turn(axis)[0];
            const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(delta)];
            pose.pos = remap.at(pose.pos);
            pose.ori = composeRow[static_cast<std::size_t>(pose.ori)];
        }

    } // namespace

    auto sameIndex3(const index3& a, const index3& b) -> bool {
        return a.x == b.x and a.y == b.y and a.z == b.z;
    }

    auto cellOccupied(const mech::Blueprint& data, const index3& pos) -> bool {
        for (const auto& cell : data.cells) {
            if (sameIndex3(cell.pose.pos, pos))
                return true;
        }
        return false;
    }

    void clear(Store& store) {
        store.aliases.clear();
        store.pendingRestore.clear();
    }

    void selectAll(Reading context, Store& store, const std::vector<QuarkActor>& actors) {
        store.aliases.clear();
        store.pendingRestore.clear();
        for (const auto& actor : actors) {
            if (const auto alias = actorAlias(context, actor))
                addAlias(store.aliases, *alias);
        }
    }

    void resetClipboard(Store& store) {
        store.clipboard = mech::Blueprint{.name = "clipboard", .author = {}, .cells = {}};
        store.focus = Focus::selection;
    }

    void toggleFocus(Store& store) {
        if (clipboardEmpty(store)) {
            store.focus = Focus::selection;
            return;
        }
        store.focus = store.focus == Focus::clipboard ? Focus::selection : Focus::clipboard;
    }

    auto clipboardEmpty(const Store& store) -> bool {
        return store.clipboard.cells.empty();
    }

    auto canPaste(const mech::Blueprint& target, const mech::Blueprint& clipboard) -> bool {
        if (clipboard.cells.empty())
            return false;
        for (const auto& cell : clipboard.cells) {
            if (cellOccupied(target, cell.pose.pos))
                return false;
        }
        return true;
    }

    void rematchAfterSync(Reading context, Store& store, const std::vector<QuarkActor>& actors) {
        if (store.pendingRestore.empty())
            return;
        restoreAliases(context, actors, store.pendingRestore, store.aliases);
        store.pendingRestore.clear();
    }

    void expand(Reading context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors) {
        if (store.aliases.empty() or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return;
        const auto cells = selectedCellIndices(context, store, actors);
        if (cells.empty())
            return;
        for (const auto& actor : actors) {
            if (not cells.contains(actor.cell))
                continue;
            if (const auto alias = actorAlias(context, actor))
                addAlias(store.aliases, *alias);
        }
    }

    void copyToClipboard(Reading context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors) {
        resetClipboard(store);
        if (store.aliases.empty() or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return;
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, hovered).data;
        const auto cells = selectedCellIndices(context, store, actors);
        for (const auto cellIndex : cells) {
            if (cellIndex < data.cells.size())
                store.clipboard.cells.push_back(data.cells[cellIndex]);
        }
    }

    auto eraseSelected(Writing context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors) -> bool {
        if (store.aliases.empty() or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return false;

        std::map<std::size_t, std::set<std::size_t>> knots;
        std::map<std::size_t, std::set<std::size_t>> halfChords;
        std::map<std::size_t, std::set<std::size_t>> walls;
        for (const auto alias : store.aliases) {
            const auto actor = findActorByAlias(context, actors, alias);
            if (not actor)
                continue;
            switch (actor->kind) {
                case QuarkActor::Kind::knot: knots[actor->cell].insert(actor->index); break;
                case QuarkActor::Kind::halfChord: halfChords[actor->cell].insert(actor->index); break;
                case QuarkActor::Kind::wall: walls[actor->cell].insert(actor->index); break;
            }
        }
        clear(store);
        if (knots.empty() and halfChords.empty() and walls.empty())
            return false;

        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
        std::set<std::size_t> dropCells;
        for (const auto& [cellIndex, indices] : knots) {
            if (cellIndex < data->data.cells.size())
                eraseDescending(data->data.cells[cellIndex].frame.corners, indices);
        }
        for (const auto& [cellIndex, indices] : halfChords) {
            if (cellIndex < data->data.cells.size())
                eraseDescending(data->data.cells[cellIndex].frame.halfribs, indices);
        }
        for (const auto& [cellIndex, indices] : walls) {
            if (cellIndex < data->data.cells.size())
                eraseDescending(data->data.cells[cellIndex].hull.membranes, indices);
        }
        for (std::size_t i = 0; i < data->data.cells.size(); ++i) {
            if (cellEmpty(data->data.cells[i]))
                dropCells.insert(i);
        }
        eraseDescending(data->data.cells, dropCells);
        return true;
    }

    auto rotateSelected(Writing context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, mech::space::orient::Semiaxis axis) -> bool {
        if (store.aliases.empty() or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return false;
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, hovered).data;
        const auto cellIndices = selectedCellIndices(context, store, actors);
        if (cellIndices.empty())
            return false;

        std::set<index3, CellPosLess> positions;
        for (const auto cellIndex : cellIndices) {
            if (cellIndex < data.cells.size())
                positions.insert(data.cells[cellIndex].pose.pos);
        }
        const auto remap = remapCells(positions, axis);
        for (const auto& [from, to] : remap) {
            if (positions.contains(to))
                continue;
            if (cellOccupied(data, to))
                return false;
        }

        store.pendingRestore = selectionRefs(context, actors, store.aliases);
        {
            auto writable = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
            for (const auto cellIndex : cellIndices) {
                if (cellIndex >= writable->data.cells.size())
                    continue;
                rotateCellPose(writable->data.cells[cellIndex].pose, axis, remap);
            }
        }
        return true;
    }

    auto moveSelected(Writing context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, index3 step) -> bool {
        if (store.aliases.empty() or (step.x == 0 and step.y == 0 and step.z == 0))
            return false;
        if (not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return false;
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, hovered).data;
        const auto cellIndices = selectedCellIndices(context, store, actors);
        if (cellIndices.empty())
            return false;

        std::set<index3, CellPosLess> positions;
        for (const auto cellIndex : cellIndices) {
            if (cellIndex < data.cells.size())
                positions.insert(data.cells[cellIndex].pose.pos);
        }
        for (const auto& from : positions) {
            const auto to = addIndex3(from, step);
            if (positions.contains(to))
                continue;
            if (cellOccupied(data, to))
                return false;
        }

        store.pendingRestore = selectionRefs(context, actors, store.aliases);
        {
            auto writable = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
            for (const auto cellIndex : cellIndices) {
                if (cellIndex >= writable->data.cells.size())
                    continue;
                writable->data.cells[cellIndex].pose.pos = addIndex3(writable->data.cells[cellIndex].pose.pos, step);
            }
        }
        return true;
    }

    auto rotateClipboard(Store& store, mech::space::orient::Semiaxis axis) -> bool {
        if (clipboardEmpty(store))
            return false;
        std::set<index3, CellPosLess> positions;
        for (const auto& cell : store.clipboard.cells)
            positions.insert(cell.pose.pos);
        if (positions.empty())
            return false;
        const auto remap = remapCells(positions, axis);
        for (auto& cell : store.clipboard.cells)
            rotateCellPose(cell.pose, axis, remap);
        return true;
    }

    auto moveClipboard(Store& store, index3 step) -> bool {
        if (clipboardEmpty(store) or (step.x == 0 and step.y == 0 and step.z == 0))
            return false;
        for (auto& cell : store.clipboard.cells)
            cell.pose.pos = addIndex3(cell.pose.pos, step);
        return true;
    }

    auto pasteClipboard(Writing context, Store& store, resource::blueprint::Asset::Id hovered) -> bool {
        if (clipboardEmpty(store) or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return false;
        {
            const auto& target = with<::eltanin::resource::blueprint::Asset>::get(context, hovered).data;
            if (not canPaste(target, store.clipboard))
                return false;
        }
        auto writable = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
        for (const auto& cell : store.clipboard.cells)
            writable->data.cells.push_back(cell);
        return true;
    }

    void handlePointer(Reading context, Store& store, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors, renderer::Integer32 under) {
        if (ImGui::GetIO().WantCaptureMouse or under == renderer::Integer32{0})
            return;
        const auto hit = findActorByAlias(context, actors, under);
        const bool shift = ImGui::GetIO().KeyShift;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and hit) {
            if (shift) {
                for (const auto alias : familyAliases(context, actors, *hit))
                    addAlias(store.aliases, alias);
            } else {
                addAlias(store.aliases, under);
            }
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) and hit) {
            if (shift) {
                for (const auto alias : familyAliases(context, actors, *hit))
                    removeAlias(store.aliases, alias);
            } else {
                removeAlias(store.aliases, under);
            }
        }
    }

    auto handleHotkeys(Writing context, Store& store, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool {
        if (store.focus != Focus::selection)
            return false;
        if (not hovered.exists())
            return false;
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) and not ImGui::GetIO().WantCaptureKeyboard)
            return eraseSelected(context, store, *hovered, actors);
        if (store.aliases.empty() or ImGui::GetIO().WantCaptureKeyboard)
            return false;
        using Semiaxis = mech::space::orient::Semiaxis;
        const bool shift = ImGui::GetIO().KeyShift;
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl)
            return false;
        if (not shift) {
            base::maybe<index3> step;
            if (ImGui::IsKeyPressed(ImGuiKey_A)) step = index3{.x = 0, .y = 0, .z = -1};
            else if (ImGui::IsKeyPressed(ImGuiKey_D)) step = index3{.x = 0, .y = 0, .z = 1};
            else if (ImGui::IsKeyPressed(ImGuiKey_W)) step = index3{.x = 1, .y = 0, .z = 0};
            else if (ImGui::IsKeyPressed(ImGuiKey_S)) step = index3{.x = -1, .y = 0, .z = 0};
            else if (ImGui::IsKeyPressed(ImGuiKey_Q)) step = index3{.x = 0, .y = -1, .z = 0};
            else if (ImGui::IsKeyPressed(ImGuiKey_E)) step = index3{.x = 0, .y = 1, .z = 0};
            if (step)
                return moveSelected(context, store, *hovered, actors, *step);
            return false;
        }
        base::maybe<Semiaxis> axis;
        if (ImGui::IsKeyPressed(ImGuiKey_A)) axis = Semiaxis::Yn;
        else if (ImGui::IsKeyPressed(ImGuiKey_D)) axis = Semiaxis::Yp;
        else if (ImGui::IsKeyPressed(ImGuiKey_W)) axis = Semiaxis::Xp;
        else if (ImGui::IsKeyPressed(ImGuiKey_S)) axis = Semiaxis::Xn;
        else if (ImGui::IsKeyPressed(ImGuiKey_Q)) axis = Semiaxis::Zn;
        else if (ImGui::IsKeyPressed(ImGuiKey_E)) axis = Semiaxis::Zp;
        if (axis)
            return rotateSelected(context, store, *hovered, actors, *axis);
        return false;
    }

    auto handleClipboardHotkeys(Store& store) -> bool {
        if (store.focus != Focus::clipboard)
            return false;
        if (clipboardEmpty(store)) {
            store.focus = Focus::selection;
            return false;
        }
        if (ImGui::GetIO().WantCaptureKeyboard)
            return false;
        using Semiaxis = mech::space::orient::Semiaxis;
        const bool shift = ImGui::GetIO().KeyShift;
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl)
            return false;
        if (not shift) {
            base::maybe<index3> step;
            if (ImGui::IsKeyPressed(ImGuiKey_A)) step = index3{.x = 0, .y = 0, .z = -1};
            else if (ImGui::IsKeyPressed(ImGuiKey_D)) step = index3{.x = 0, .y = 0, .z = 1};
            else if (ImGui::IsKeyPressed(ImGuiKey_W)) step = index3{.x = 1, .y = 0, .z = 0};
            else if (ImGui::IsKeyPressed(ImGuiKey_S)) step = index3{.x = -1, .y = 0, .z = 0};
            else if (ImGui::IsKeyPressed(ImGuiKey_Q)) step = index3{.x = 0, .y = -1, .z = 0};
            else if (ImGui::IsKeyPressed(ImGuiKey_E)) step = index3{.x = 0, .y = 1, .z = 0};
            if (step)
                return moveClipboard(store, *step);
            return false;
        }
        base::maybe<Semiaxis> axis;
        if (ImGui::IsKeyPressed(ImGuiKey_A)) axis = Semiaxis::Yn;
        else if (ImGui::IsKeyPressed(ImGuiKey_D)) axis = Semiaxis::Yp;
        else if (ImGui::IsKeyPressed(ImGuiKey_W)) axis = Semiaxis::Xp;
        else if (ImGui::IsKeyPressed(ImGuiKey_S)) axis = Semiaxis::Xn;
        else if (ImGui::IsKeyPressed(ImGuiKey_Q)) axis = Semiaxis::Zn;
        else if (ImGui::IsKeyPressed(ImGuiKey_E)) axis = Semiaxis::Zp;
        if (axis)
            return rotateClipboard(store, *axis);
        return false;
    }

    auto handleClipboardChords(Writing context, Store& store, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool {
        if (ImGui::GetIO().WantCaptureKeyboard or not ImGui::GetIO().KeyCtrl)
            return false;
        if (ImGui::IsKeyPressed(ImGuiKey_C)) {
            if (hovered.exists() and not store.aliases.empty())
                copyToClipboard(context, store, *hovered, actors);
            return false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V) and hovered.exists())
            return pasteClipboard(context, store, *hovered);
        return false;
    }

    auto drawPanel(Writing context, Store& store, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) -> bool {
        ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 220.0f}, ImGuiCond_FirstUseEver);
        bool erased = false;
        if (ImGui::Begin("Selection")) {
            if (store.aliases.empty()) {
                if (ImGui::Button("select all") and not actors.empty())
                    selectAll(context, store, actors);
            } else {
                if (ImGui::Button("expand") and hovered.exists())
                    expand(context, store, *hovered, actors);
                ImGui::SameLine();
                if (ImGui::Button("copy") and hovered.exists())
                    copyToClipboard(context, store, *hovered, actors);
                ImGui::SameLine();
                if (ImGui::Button("deselect"))
                    clear(store);
                ImGui::SameLine();
                if (ImGui::Button("delete") and hovered.exists())
                    erased = eraseSelected(context, store, *hovered, actors);
                ImGui::Separator();
                ImGui::TextDisabled("Del — remove · x/RMB deselect · Shift+LMB family · WASD/QE move · Shift rotate · Ctrl+C/V");
                const auto* blueprintData = (hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *hovered))
                    ? &with<::eltanin::resource::blueprint::Asset>::get(context, *hovered).data
                    : nullptr;

                const auto byAlias = actorByAliasIndex(context, actors);
                std::map<index3, std::vector<renderer::Integer32>, CellPosLess> byCell;
                std::vector<renderer::Integer32> orphans;
                for (const auto alias : store.aliases) {
                    const auto found = byAlias.find(alias);
                    if (blueprintData and found != byAlias.end() and found->second->cell < blueprintData->cells.size()) {
                        byCell[blueprintData->cells[found->second->cell].pose.pos].push_back(alias);
                        continue;
                    }
                    orphans.push_back(alias);
                }

                const auto drawLeaf = [&](renderer::Integer32 alias) {
                    base::maybe<QuarkActor> actor;
                    if (const auto found = byAlias.find(alias); found != byAlias.end())
                        actor = *found->second;
                    const auto row = selectionRowLabel(blueprintData, actor, alias);
                    ImGui::PushID(static_cast<int>(static_cast<fqsm::internal::id::BaseType>(alias)));
                    ImGui::TextUnformatted(row.c_str());
                    ImGui::SameLine();
                    const bool remove = ImGui::SmallButton("x");
                    ImGui::PopID();
                    if (remove)
                        removeAlias(store.aliases, alias);
                };

                for (const auto& [pos, group] : byCell) {
                    ImGui::PushID(pos.x);
                    ImGui::PushID(pos.y);
                    ImGui::PushID(pos.z);
                    const bool open = ImGui::TreeNode("cell", "Cell(%d,%d,%d) (%zu)", pos.x, pos.y, pos.z, group.size());
                    ImGui::SameLine();
                    const bool dropFamily = ImGui::SmallButton("x");
                    if (dropFamily) {
                        for (const auto alias : group)
                            removeAlias(store.aliases, alias);
                    }
                    if (open) {
                        if (not dropFamily) {
                            for (const auto alias : group)
                                drawLeaf(alias);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    ImGui::PopID();
                    ImGui::PopID();
                }
                for (const auto alias : orphans)
                    drawLeaf(alias);
            }
        }
        ImGui::End();
        return erased;
    }

    auto drawClipboardPanel(Writing context, Store& store, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered) -> bool {
        ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y + 228.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 140.0f}, ImGuiCond_FirstUseEver);
        bool pasted = false;
        if (ImGui::Begin("Clipboard")) {
            std::size_t knots = 0;
            std::size_t halfChords = 0;
            std::size_t walls = 0;
            for (const auto& cell : store.clipboard.cells) {
                knots += cell.frame.corners.size();
                halfChords += cell.frame.halfribs.size();
                walls += cell.hull.membranes.size();
            }
            ImGui::TextDisabled("%zu cells · %zu knots · %zu half-chords · %zu walls", store.clipboard.cells.size(), knots, halfChords, walls);
            const bool empty = clipboardEmpty(store);
            bool allowed = false;
            if (not empty and hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *hovered))
                allowed = canPaste(with<::eltanin::resource::blueprint::Asset>::get(context, *hovered).data, store.clipboard);
            if (empty) {
                ImGui::TextDisabled("empty");
                ImGui::TextDisabled("Ctrl+C copy selection");
            } else {
                if (allowed)
                    ImGui::TextUnformatted("can paste");
                else
                    ImGui::TextUnformatted("blocked");
                const char* focusLabel = store.focus == Focus::clipboard ? "focus: buffer (B)" : "focus: selection (B)";
                ImGui::TextDisabled("%s", focusLabel);
                if (ImGui::Button("paste") and allowed and hovered.exists())
                    pasted = pasteClipboard(context, store, *hovered);
                ImGui::SameLine();
                if (ImGui::Button("clear"))
                    resetClipboard(store);
            }
        }
        ImGui::End();
        return pasted;
    }

} // namespace eltanin::views::blueprints::selection
