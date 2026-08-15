#include "views/blueprints/selection.h"

#include "mech/semantics/quarks.h"
#include "mech/semantics/subframe.h"

#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/mount.q1.h>
#include <fQSM/identifier.h>
#include <rmmr/resources/manager.q1.h>
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

        auto findQuarkByAlias(Reading context, const std::vector<QuarkActor>& actors, renderer::Integer32 alias) -> base::maybe<QuarkActor> {
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias == alias)
                    return actor;
            }
            return {};
        }

        auto findMountByAlias(Reading context, const std::vector<MountActor>& actors, renderer::Integer32 alias) -> base::maybe<MountActor> {
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                if (with<scene::actor::Identified>::get(context, actor.id).scenicAlias == alias)
                    return actor;
            }
            return {};
        }

        auto quarkByAliasIndex(Reading context, const std::vector<QuarkActor>& actors) -> std::unordered_map<renderer::Integer32, const QuarkActor*> {
            std::unordered_map<renderer::Integer32, const QuarkActor*> out;
            out.reserve(actors.size());
            for (const auto& actor : actors) {
                if (not with<scene::actor::Identified>::exists(context, actor.id))
                    continue;
                out.emplace(with<scene::actor::Identified>::get(context, actor.id).scenicAlias, &actor);
            }
            return out;
        }

        auto mountByAliasIndex(Reading context, const std::vector<MountActor>& actors) -> std::unordered_map<renderer::Integer32, const MountActor*> {
            std::unordered_map<renderer::Integer32, const MountActor*> out;
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

        auto quarkRowLabel(const Blueprint* data, const base::maybe<QuarkActor>& actor, renderer::Integer32 alias) -> std::string {
            const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
            if (not actor or not data or actor->cell >= data->cells.size())
                return std::format("?  #{}", hash);
            const auto& cell = data->cells[actor->cell];
            const auto& pos = cell.placement.cell;
            switch (actor->kind) {
                case QuarkActor::Kind::knot: {
                    if (actor->index >= cell.corners.size())
                        return std::format("knot[{}:{}]  #{}", actor->cell, actor->index, hash);
                    const auto& knot = cell.corners[actor->index];
                    return std::format("knot[{}:{}] {}  cell[{},{},{}] localOri={}  #{}", actor->cell, actor->index, mech::skeleton::cornerSpecs.at(knot.kind).code, pos.x, pos.y, pos.z, knot.ori, hash);
                }
                case QuarkActor::Kind::halfChord: {
                    if (actor->index >= cell.halfribs.size())
                        return std::format("halfChord[{}:{}]  #{}", actor->cell, actor->index, hash);
                    const auto& halfChord = cell.halfribs[actor->index];
                    const char pole = halfChord.pole == mech::skeleton::Halfrib::Pole::starts ? 's' : 'e';
                    return std::format("halfChord[{}:{}] {}:{}  cell[{},{},{}] localOri={}  #{}", actor->cell, actor->index, mech::skeleton::halfribSpecs.at(halfChord.kind).code, pole, pos.x, pos.y, pos.z, halfChord.ori, hash);
                }
                case QuarkActor::Kind::wall: {
                    if (actor->index >= cell.membranes.size())
                        return std::format("wall[{}:{}]  #{}", actor->cell, actor->index, hash);
                    const auto& wall = cell.membranes[actor->index];
                    return std::format("wall[{}:{}] {}  cell[{},{},{}] localOri={}  #{}", actor->cell, actor->index, mech::skeleton::membraneSpecs.at(wall.kind).code, pos.x, pos.y, pos.z, wall.ori, hash);
                }
            }
            return std::format("?  #{}", hash);
        }

        auto mountRowLabel(const Blueprint* data, const base::maybe<MountActor>& actor, renderer::Integer32 alias) -> std::string {
            const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
            if (not actor or not data or actor->index >= data->mounts.size())
                return std::format("mount?  #{}", hash);
            const auto& placed = data->mounts[actor->index];
            const auto& grid = placed.transform.grid;
            return std::format("mount[{}] {}  grid[{},{},{}] ori={}  #{}", actor->index, placed.mount.text(), grid.x, grid.y, grid.z, placed.transform.rotation, hash);
        }

        auto selectionQuarkRefs(Reading context, const std::vector<QuarkActor>& actors, const std::vector<renderer::Integer32>& aliases) -> std::vector<QuarkRef> {
            std::vector<QuarkRef> refs;
            for (const auto alias : aliases) {
                if (const auto actor = findQuarkByAlias(context, actors, alias))
                    refs.push_back(QuarkRef{.kind = actor->kind, .cell = actor->cell, .index = actor->index});
            }
            return refs;
        }

        auto selectionMountRefs(Reading context, const std::vector<MountActor>& actors, const std::vector<renderer::Integer32>& aliases) -> std::vector<MountRef> {
            std::vector<MountRef> refs;
            for (const auto alias : aliases) {
                if (const auto actor = findMountByAlias(context, actors, alias))
                    refs.push_back(MountRef{.index = actor->index});
            }
            return refs;
        }

        void restoreQuarkAliases(Reading context, const std::vector<QuarkActor>& actors, const std::vector<QuarkRef>& refs, std::vector<renderer::Integer32>& aliases) {
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

        void restoreMountAliases(Reading context, const std::vector<MountActor>& actors, const std::vector<MountRef>& refs, std::vector<renderer::Integer32>& aliases) {
            for (const auto& ref : refs) {
                for (const auto& actor : actors) {
                    if (actor.index != ref.index)
                        continue;
                    if (not with<scene::actor::Identified>::exists(context, actor.id))
                        break;
                    aliases.push_back(with<scene::actor::Identified>::get(context, actor.id).scenicAlias);
                    break;
                }
            }
        }

        auto actorAlias(Reading context, scene::actor::Mesh::Id id) -> base::maybe<renderer::Integer32> {
            if (not with<scene::actor::Identified>::exists(context, id))
                return {};
            return with<scene::actor::Identified>::get(context, id).scenicAlias;
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
                if (const auto alias = actorAlias(context, actor.id))
                    out.push_back(*alias);
            }
            return out;
        }

        auto selectedCellIndices(Reading context, const Store& store, const std::vector<QuarkActor>& actors) -> std::set<std::size_t> {
            std::set<std::size_t> cells;
            for (const auto alias : store.aliases) {
                if (const auto actor = findQuarkByAlias(context, actors, alias))
                    cells.insert(actor->cell);
            }
            return cells;
        }

        auto selectedMountIndices(Reading context, const Store& store, const std::vector<MountActor>& actors) -> std::set<std::size_t> {
            std::set<std::size_t> mounts;
            for (const auto alias : store.aliases) {
                if (const auto actor = findMountByAlias(context, actors, alias))
                    mounts.insert(actor->index);
            }
            return mounts;
        }

        auto cellEmpty(const Cell& cell) -> bool {
            return cell.corners.empty() and cell.halfribs.empty() and cell.membranes.empty();
        }

        auto rotateCellPose(mech::space::cell::Placement& pose, mech::space::orient::Semiaxis axis, const std::map<index3, index3, CellPosLess>& remap) -> void {
            const auto delta = mech::space::orient::turn(axis)[0];
            const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(delta)];
            pose.cell = remap.at(pose.cell);
            pose.ori = composeRow[static_cast<std::size_t>(pose.ori)];
        }

        auto rotateMountTransform(mech::space::Transform& transform, mech::space::orient::Semiaxis axis, const index3& pivot) -> void {
            const auto delta = mech::space::orient::turn(axis)[0];
            const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(delta)];
            transform.grid = rotateCellPos(transform.grid, pivot, axis);
            transform.rotation = composeRow[static_cast<std::size_t>(transform.rotation)];
        }

    } // namespace

    auto sameIndex3(const index3& a, const index3& b) -> bool {
        return a.x == b.x and a.y == b.y and a.z == b.z;
    }

    auto cellOccupied(const Blueprint& data, const index3& pos) -> bool {
        for (const auto& cell : data.cells) {
            if (sameIndex3(cell.placement.cell, pos))
                return true;
        }
        return false;
    }

    void clear(Store& store) {
        store.aliases.clear();
        store.pendingRestore.clear();
        store.pendingMountRestore.clear();
    }

    void selectAll(Reading context, Store& store, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) {
        store.aliases.clear();
        store.pendingRestore.clear();
        store.pendingMountRestore.clear();
        for (const auto& actor : quarks) {
            if (const auto alias = actorAlias(context, actor.id))
                addAlias(store.aliases, *alias);
        }
        for (const auto& actor : mounts) {
            if (const auto alias = actorAlias(context, actor.id))
                addAlias(store.aliases, *alias);
        }
    }

    void resetClipboard(Store& store) {
        store.clipboard = Blueprint{.name = "clipboard", .author = {}, .cells = {}, .mounts = {}, .file = {}};
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
        return store.clipboard.cells.empty() and store.clipboard.mounts.empty();
    }

    auto canPaste(const Blueprint& target, const Blueprint& clipboard) -> bool {
        if (clipboard.cells.empty() and clipboard.mounts.empty())
            return false;
        for (const auto& cell : clipboard.cells) {
            if (cellOccupied(target, cell.placement.cell))
                return false;
        }
        return true;
    }

    void rematchAfterSync(Reading context, Store& store, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) {
        if (store.pendingRestore.empty() and store.pendingMountRestore.empty())
            return;
        store.aliases.clear();
        restoreQuarkAliases(context, quarks, store.pendingRestore, store.aliases);
        restoreMountAliases(context, mounts, store.pendingMountRestore, store.aliases);
        store.pendingRestore.clear();
        store.pendingMountRestore.clear();
    }

    void expand(Reading context, Store& store, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>&) {
        if (store.aliases.empty() or not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return;
        const auto cells = selectedCellIndices(context, store, quarks);
        if (cells.empty())
            return;
        for (const auto& actor : quarks) {
            if (not cells.contains(actor.cell))
                continue;
            if (const auto alias = actorAlias(context, actor.id))
                addAlias(store.aliases, *alias);
        }
    }

    void copyToClipboard(Reading context, Store& store, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) {
        resetClipboard(store);
        if (store.aliases.empty() or not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, hovered);
        for (const auto cellIndex : selectedCellIndices(context, store, quarks)) {
            if (cellIndex < data.cells.size())
                store.clipboard.cells.push_back(data.cells[cellIndex]);
        }
        for (const auto mountIndex : selectedMountIndices(context, store, mounts)) {
            if (mountIndex < data.mounts.size())
                store.clipboard.mounts.push_back(data.mounts[mountIndex]);
        }
    }

    auto eraseSelected(Writing context, Store& store, history::Store& history, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) -> bool {
        if (store.aliases.empty() or not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return false;

        std::map<std::size_t, std::set<std::size_t>> knots;
        std::map<std::size_t, std::set<std::size_t>> halfChords;
        std::map<std::size_t, std::set<std::size_t>> walls;
        std::set<std::size_t> dropMounts;
        for (const auto alias : store.aliases) {
            if (const auto actor = findQuarkByAlias(context, quarks, alias)) {
                switch (actor->kind) {
                    case QuarkActor::Kind::knot: knots[actor->cell].insert(actor->index); break;
                    case QuarkActor::Kind::halfChord: halfChords[actor->cell].insert(actor->index); break;
                    case QuarkActor::Kind::wall: walls[actor->cell].insert(actor->index); break;
                }
                continue;
            }
            if (const auto actor = findMountByAlias(context, mounts, alias))
                dropMounts.insert(actor->index);
        }
        clear(store);
        if (knots.empty() and halfChords.empty() and walls.empty() and dropMounts.empty())
            return false;

        history::record(history, hovered, "erase selection", with<::eltanin::mech::Blueprint>::get(context, hovered));
        auto data = with<::eltanin::mech::Blueprint>::modify(context, hovered);
        std::set<std::size_t> dropCells;
        for (const auto& [cellIndex, indices] : knots) {
            if (cellIndex < data->cells.size())
                eraseDescending(data->cells[cellIndex].corners, indices);
        }
        for (const auto& [cellIndex, indices] : halfChords) {
            if (cellIndex < data->cells.size())
                eraseDescending(data->cells[cellIndex].halfribs, indices);
        }
        for (const auto& [cellIndex, indices] : walls) {
            if (cellIndex < data->cells.size())
                eraseDescending(data->cells[cellIndex].membranes, indices);
        }
        for (std::size_t i = 0; i < data->cells.size(); ++i) {
            if (cellEmpty(data->cells[i]))
                dropCells.insert(i);
        }
        eraseDescending(data->cells, dropCells);
        eraseDescending(data->mounts, dropMounts);
        return true;
    }

    auto rotateSelected(Writing context, Store& store, history::Store& history, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts, mech::space::orient::Semiaxis axis) -> bool {
        if (store.aliases.empty() or not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return false;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, hovered);
        const auto cellIndices = selectedCellIndices(context, store, quarks);
        const auto mountIndices = selectedMountIndices(context, store, mounts);
        if (cellIndices.empty() and mountIndices.empty())
            return false;

        std::set<index3, CellPosLess> positions;
        for (const auto cellIndex : cellIndices) {
            if (cellIndex < data.cells.size())
                positions.insert(data.cells[cellIndex].placement.cell);
        }
        for (const auto mountIndex : mountIndices) {
            if (mountIndex < data.mounts.size())
                positions.insert(data.mounts[mountIndex].transform.grid);
        }
        if (positions.empty())
            return false;

        const auto remap = remapCells(positions, axis);
        const auto pivot = cellsPivot(positions);
        for (const auto cellIndex : cellIndices) {
            if (cellIndex >= data.cells.size())
                continue;
            const auto to = remap.at(data.cells[cellIndex].placement.cell);
            if (positions.contains(to))
                continue;
            if (cellOccupied(data, to))
                return false;
        }

        store.pendingRestore = selectionQuarkRefs(context, quarks, store.aliases);
        store.pendingMountRestore = selectionMountRefs(context, mounts, store.aliases);
        history::record(history, hovered, "rotate selection", data);
        {
            auto writable = with<::eltanin::mech::Blueprint>::modify(context, hovered);
            for (const auto cellIndex : cellIndices) {
                if (cellIndex >= writable->cells.size())
                    continue;
                rotateCellPose(writable->cells[cellIndex].placement, axis, remap);
            }
            for (const auto mountIndex : mountIndices) {
                if (mountIndex >= writable->mounts.size())
                    continue;
                rotateMountTransform(writable->mounts[mountIndex].transform, axis, pivot);
            }
        }
        return true;
    }

    auto moveSelected(Writing context, Store& store, history::Store& history, mech::Blueprint::Id hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts, index3 step) -> bool {
        if (store.aliases.empty() or (step.x == 0 and step.y == 0 and step.z == 0))
            return false;
        if (not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return false;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, hovered);
        const auto cellIndices = selectedCellIndices(context, store, quarks);
        const auto mountIndices = selectedMountIndices(context, store, mounts);
        if (cellIndices.empty() and mountIndices.empty())
            return false;

        std::set<index3, CellPosLess> positions;
        for (const auto cellIndex : cellIndices) {
            if (cellIndex < data.cells.size())
                positions.insert(data.cells[cellIndex].placement.cell);
        }
        for (const auto& from : positions) {
            const auto to = addIndex3(from, step);
            if (positions.contains(to))
                continue;
            if (cellOccupied(data, to))
                return false;
        }

        store.pendingRestore = selectionQuarkRefs(context, quarks, store.aliases);
        store.pendingMountRestore = selectionMountRefs(context, mounts, store.aliases);
        history::record(history, hovered, "move selection", data);
        {
            auto writable = with<::eltanin::mech::Blueprint>::modify(context, hovered);
            for (const auto cellIndex : cellIndices) {
                if (cellIndex >= writable->cells.size())
                    continue;
                writable->cells[cellIndex].placement.cell = addIndex3(writable->cells[cellIndex].placement.cell, step);
            }
            for (const auto mountIndex : mountIndices) {
                if (mountIndex >= writable->mounts.size())
                    continue;
                writable->mounts[mountIndex].transform.grid = addIndex3(writable->mounts[mountIndex].transform.grid, step);
            }
        }
        return true;
    }

    auto rotateClipboard(Store& store, mech::space::orient::Semiaxis axis) -> bool {
        if (clipboardEmpty(store))
            return false;
        std::set<index3, CellPosLess> positions;
        for (const auto& cell : store.clipboard.cells)
            positions.insert(cell.placement.cell);
        for (const auto& placed : store.clipboard.mounts)
            positions.insert(placed.transform.grid);
        if (positions.empty())
            return false;
        const auto remap = remapCells(positions, axis);
        const auto pivot = cellsPivot(positions);
        for (auto& cell : store.clipboard.cells)
            rotateCellPose(cell.placement, axis, remap);
        for (auto& placed : store.clipboard.mounts)
            rotateMountTransform(placed.transform, axis, pivot);
        return true;
    }

    auto moveClipboard(Store& store, index3 step) -> bool {
        if (clipboardEmpty(store) or (step.x == 0 and step.y == 0 and step.z == 0))
            return false;
        for (auto& cell : store.clipboard.cells)
            cell.placement.cell = addIndex3(cell.placement.cell, step);
        for (auto& placed : store.clipboard.mounts)
            placed.transform.grid = addIndex3(placed.transform.grid, step);
        return true;
    }

    auto pasteClipboard(Writing context, Store& store, history::Store& history, mech::Blueprint::Id hovered) -> bool {
        if (clipboardEmpty(store) or not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return false;
        {
            const auto& target = with<::eltanin::mech::Blueprint>::get(context, hovered);
            if (not canPaste(target, store.clipboard))
                return false;
        }
        history::record(history, hovered, "paste", with<::eltanin::mech::Blueprint>::get(context, hovered));
        auto writable = with<::eltanin::mech::Blueprint>::modify(context, hovered);
        for (const auto& cell : store.clipboard.cells)
            writable->cells.push_back(cell);
        for (const auto& placed : store.clipboard.mounts)
            writable->mounts.push_back(placed);
        return true;
    }

    auto hitMount(Reading context, const std::vector<MountActor>& mounts, renderer::Integer32 under) -> base::maybe<MountActor> {
        if (under == renderer::Integer32{0})
            return {};
        return findMountByAlias(context, mounts, under);
    }

    auto soleSelectedMountIndex(Reading context, const Store& store, const std::vector<MountActor>& mounts) -> base::maybe<std::size_t> {
        const auto mountIndices = selectedMountIndices(context, store, mounts);
        if (mountIndices.size() != 1)
            return {};
        return *mountIndices.begin();
    }

    auto setSoleMountTransform(Writing context, Store& store, history::Store& history, mech::Blueprint::Id hovered, const std::vector<MountActor>& mounts, const mech::space::Transform& transform) -> bool {
        if (not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return false;
        const auto mountIndex = soleSelectedMountIndex(context, store, mounts);
        if (not mountIndex)
            return false;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, hovered);
        if (*mountIndex >= data.mounts.size())
            return false;
        const auto& current = data.mounts[*mountIndex].transform;
        if (current.grid.x == transform.grid.x and current.grid.y == transform.grid.y and current.grid.z == transform.grid.z and current.rotation == transform.rotation)
            return false;
        store.pendingRestore.clear();
        store.pendingMountRestore = selectionMountRefs(context, mounts, store.aliases);
        history::record(history, hovered, "orient mount", data);
        with<::eltanin::mech::Blueprint>::modify(context, hovered)->mounts[*mountIndex].transform = transform;
        return true;
    }

    auto setSoleMountUnit(Writing context, Store& store, history::Store& history, mech::Blueprint::Id hovered, const std::vector<MountActor>& mounts, rmmr::resource::Unit::Name name, base::maybe<mech::space::Transform> transform) -> bool {
        if (not with<::eltanin::mech::Blueprint>::exists(context, hovered))
            return false;
        const auto mountIndex = soleSelectedMountIndex(context, store, mounts);
        if (not mountIndex)
            return false;
        const auto& data = with<::eltanin::mech::Blueprint>::get(context, hovered);
        if (*mountIndex >= data.mounts.size())
            return false;
        const auto& current = data.mounts[*mountIndex];
        const bool sameName = current.mount == name;
        const bool sameTransform = not transform.has_value() or (current.transform.grid.x == transform->grid.x and current.transform.grid.y == transform->grid.y and current.transform.grid.z == transform->grid.z and current.transform.rotation == transform->rotation);
        if (sameName and sameTransform)
            return false;
        store.pendingRestore.clear();
        store.pendingMountRestore = selectionMountRefs(context, mounts, store.aliases);
        history::record(history, hovered, "replace mount", data);
        auto writable = with<::eltanin::mech::Blueprint>::modify(context, hovered);
        writable->mounts[*mountIndex].mount = std::move(name);
        if (transform.has_value())
            writable->mounts[*mountIndex].transform = *transform;
        return true;
    }

    void handlePointer(Reading context, Store& store, base::maybe<mech::Blueprint::Id>, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts, renderer::Integer32 under) {
        if (ImGui::GetIO().WantCaptureMouse)
            return;
        base::maybe<QuarkActor> quarkHit;
        base::maybe<MountActor> mountHit;
        if (under != renderer::Integer32{0}) {
            quarkHit = findQuarkByAlias(context, quarks, under);
            mountHit = findMountByAlias(context, mounts, under);
        }
        const bool shift = ImGui::GetIO().KeyShift;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (not quarkHit and not mountHit)
                return;
            if (quarkHit and shift) {
                for (const auto alias : familyAliases(context, quarks, *quarkHit))
                    addAlias(store.aliases, alias);
            } else {
                addAlias(store.aliases, under);
            }
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (not quarkHit and not mountHit) {
                clear(store);
                return;
            }
            if (quarkHit and shift) {
                for (const auto alias : familyAliases(context, quarks, *quarkHit))
                    removeAlias(store.aliases, alias);
            } else {
                removeAlias(store.aliases, under);
            }
        }
    }

    auto handleHotkeys(Writing context, Store& store, history::Store& history, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) -> bool {
        if (ImGui::GetIO().WantCaptureKeyboard)
            return false;
        // / — deselect all; if selection already empty — clear clipboard.
        if (ImGui::IsKeyPressed(ImGuiKey_Slash)) {
            if (not store.aliases.empty()) {
                clear(store);
                return false;
            }
            if (not clipboardEmpty(store)) {
                resetClipboard(store);
                return true;
            }
            return false;
        }
        if (store.focus != Focus::selection)
            return false;
        if (not hovered.has_value())
            return false;
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            return eraseSelected(context, store, history, *hovered, quarks, mounts);
        if (store.aliases.empty())
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
                return moveSelected(context, store, history, *hovered, quarks, mounts, *step);
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
            return rotateSelected(context, store, history, *hovered, quarks, mounts, *axis);
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

    auto handleClipboardChords(Writing context, Store& store, history::Store& history, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) -> bool {
        if (ImGui::GetIO().WantCaptureKeyboard or not ImGui::GetIO().KeyCtrl)
            return false;
        if (ImGui::IsKeyPressed(ImGuiKey_C)) {
            if (hovered.has_value() and not store.aliases.empty())
                copyToClipboard(context, store, *hovered, quarks, mounts);
            return false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V) and hovered.has_value())
            return pasteClipboard(context, store, history, *hovered);
        return false;
    }

    auto drawPanel(Writing context, Store& store, history::Store& history, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<mech::Blueprint::Id> hovered, const std::vector<QuarkActor>& quarks, const std::vector<MountActor>& mounts) -> bool {
        ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 220.0f}, ImGuiCond_FirstUseEver);
        bool erased = false;
        if (ImGui::Begin("Selection")) {
            if (store.aliases.empty()) {
                if (ImGui::Button("select all") and (not quarks.empty() or not mounts.empty()))
                    selectAll(context, store, quarks, mounts);
            } else {
                if (ImGui::Button("expand") and hovered.has_value())
                    expand(context, store, *hovered, quarks, mounts);
                ImGui::SameLine();
                if (ImGui::Button("copy") and hovered.has_value())
                    copyToClipboard(context, store, *hovered, quarks, mounts);
                ImGui::SameLine();
                if (ImGui::Button("deselect"))
                    clear(store);
                ImGui::SameLine();
                if (ImGui::Button("delete") and hovered.has_value())
                    erased = eraseSelected(context, store, history, *hovered, quarks, mounts);
                ImGui::Separator();
                ImGui::TextDisabled("Del — remove · / or empty RMB deselect · empty / clears clipboard · Shift+LMB family · WASD/QE move · Shift rotate · Ctrl+C/V");
                const auto* blueprintData = (hovered.has_value() and with<::eltanin::mech::Blueprint>::exists(context, *hovered))
                    ? &with<::eltanin::mech::Blueprint>::get(context, *hovered)
                    : nullptr;

                const auto quarksByAlias = quarkByAliasIndex(context, quarks);
                const auto mountsByAlias = mountByAliasIndex(context, mounts);
                std::map<index3, std::vector<renderer::Integer32>, CellPosLess> byCell;
                std::vector<renderer::Integer32> mountAliases;
                std::vector<renderer::Integer32> orphans;
                for (const auto alias : store.aliases) {
                    if (const auto found = quarksByAlias.find(alias); blueprintData and found != quarksByAlias.end() and found->second->cell < blueprintData->cells.size()) {
                        byCell[blueprintData->cells[found->second->cell].placement.cell].push_back(alias);
                        continue;
                    }
                    if (mountsByAlias.contains(alias)) {
                        mountAliases.push_back(alias);
                        continue;
                    }
                    orphans.push_back(alias);
                }

                const auto drawQuarkLeaf = [&](renderer::Integer32 alias) {
                    base::maybe<QuarkActor> actor;
                    if (const auto found = quarksByAlias.find(alias); found != quarksByAlias.end())
                        actor = *found->second;
                    const auto row = quarkRowLabel(blueprintData, actor, alias);
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
                                drawQuarkLeaf(alias);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    ImGui::PopID();
                    ImGui::PopID();
                }

                if (not mountAliases.empty()) {
                    const bool open = ImGui::TreeNode("mounts", "Mounts (%zu)", mountAliases.size());
                    if (open) {
                        for (const auto alias : mountAliases) {
                            base::maybe<MountActor> actor;
                            if (const auto found = mountsByAlias.find(alias); found != mountsByAlias.end())
                                actor = *found->second;
                            const auto row = mountRowLabel(blueprintData, actor, alias);
                            ImGui::PushID(static_cast<int>(static_cast<fqsm::internal::id::BaseType>(alias)));
                            ImGui::TextUnformatted(row.c_str());
                            ImGui::SameLine();
                            const bool remove = ImGui::SmallButton("x");
                            ImGui::PopID();
                            if (remove)
                                removeAlias(store.aliases, alias);
                        }
                        ImGui::TreePop();
                    }
                }
                for (const auto alias : orphans)
                    drawQuarkLeaf(alias);
            }
        }
        ImGui::End();
        return erased;
    }

    auto drawClipboardPanel(Writing context, Store& store, history::Store& history, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<mech::Blueprint::Id> hovered) -> bool {
        ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y + 228.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 140.0f}, ImGuiCond_FirstUseEver);
        bool pasted = false;
        if (ImGui::Begin("Clipboard")) {
            std::size_t knots = 0;
            std::size_t halfChords = 0;
            std::size_t walls = 0;
            for (const auto& cell : store.clipboard.cells) {
                knots += cell.corners.size();
                halfChords += cell.halfribs.size();
                walls += cell.membranes.size();
            }
            ImGui::TextDisabled("%zu cells · %zu knots · %zu half-chords · %zu walls · %zu mounts", store.clipboard.cells.size(), knots, halfChords, walls, store.clipboard.mounts.size());
            const bool empty = clipboardEmpty(store);
            bool allowed = false;
            if (not empty and hovered.has_value() and with<::eltanin::mech::Blueprint>::exists(context, *hovered))
                allowed = canPaste(with<::eltanin::mech::Blueprint>::get(context, *hovered), store.clipboard);
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
                if (ImGui::Button("paste") and allowed and hovered.has_value())
                    pasted = pasteClipboard(context, store, history, *hovered);
                ImGui::SameLine();
                if (ImGui::Button("clear"))
                    resetClipboard(store);
            }
        }
        ImGui::End();
        return pasted;
    }

} // namespace eltanin::views::blueprints::selection
