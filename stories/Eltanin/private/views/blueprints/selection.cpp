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

        // World ±90° of a lattice point about pivot; R = orient::matrix[turn(axis)[0]] (same δ as family ori compose).
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

        void eraseDescending(auto& vec, const std::set<std::size_t>& indices) {
            for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
                if (*it < vec.size())
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(*it));
            }
        }

        auto quarkPose(const mech::Blueprint& data, const QuarkActor& actor) -> base::maybe<mech::space::cell::Pose> {
            switch (actor.kind) {
                case QuarkActor::Kind::knot:
                    if (actor.index < data.knots.size())
                        return data.knots[actor.index].pose;
                    break;
                case QuarkActor::Kind::halfChord:
                    if (actor.index < data.halfChords.size())
                        return data.halfChords[actor.index].pose;
                    break;
            }
            return {};
        }

        auto selectionRowLabel(const mech::Blueprint* data, const base::maybe<QuarkActor>& actor, renderer::Integer32 alias) -> std::string {
            const auto hash = fqsm::internal::id::info_hash(static_cast<fqsm::internal::id::BaseType>(alias));
            if (not actor or not data)
                return std::format("?  #{}", hash);
            switch (actor->kind) {
                case QuarkActor::Kind::knot: {
                    if (actor->index >= data->knots.size())
                        return std::format("knot[{}]  #{}", actor->index, hash);
                    const auto& knot = data->knots[actor->index];
                    const auto& pos = knot.pose.pos;
                    return std::format("knot[{}] {}  cell[{},{},{}] ori={}  #{}", actor->index, mech::subframe::corner::specs.at(knot.kind).code, pos.x, pos.y, pos.z, knot.pose.ori, hash);
                }
                case QuarkActor::Kind::halfChord: {
                    if (actor->index >= data->halfChords.size())
                        return std::format("halfChord[{}]  #{}", actor->index, hash);
                    const auto& halfChord = data->halfChords[actor->index];
                    const auto& pos = halfChord.pose.pos;
                    const char pole = halfChord.pole == mech::subframe::halfEdge::Pole::s ? 's' : 'e';
                    return std::format("halfChord[{}] {}:{}  cell[{},{},{}] ori={}  #{}", actor->index, mech::subframe::halfEdge::specs.at(halfChord.kind).code, pole, pos.x, pos.y, pos.z, halfChord.pose.ori, hash);
                }
            }
            return std::format("?  #{}", hash);
        }

        auto selectionRefs(Reading context, const std::vector<QuarkActor>& actors, const std::vector<renderer::Integer32>& aliases) -> std::vector<std::pair<QuarkActor::Kind, std::size_t>> {
            std::vector<std::pair<QuarkActor::Kind, std::size_t>> refs;
            for (const auto alias : aliases) {
                if (const auto actor = findActorByAlias(context, actors, alias))
                    refs.emplace_back(actor->kind, actor->index);
            }
            return refs;
        }

        void restoreAliases(Reading context, const std::vector<QuarkActor>& actors, const std::vector<std::pair<QuarkActor::Kind, std::size_t>>& refs, std::vector<renderer::Integer32>& aliases) {
            aliases.clear();
            for (const auto& [kind, index] : refs) {
                for (const auto& actor : actors) {
                    if (actor.kind != kind or actor.index != index)
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

        auto familyAliases(Reading context, const std::vector<QuarkActor>& actors, const mech::Blueprint& data, const QuarkActor& hit) -> std::vector<renderer::Integer32> {
            const auto hitPose = quarkPose(data, hit);
            if (not hitPose)
                return {};
            std::vector<renderer::Integer32> out;
            for (const auto& actor : actors) {
                const auto pose = quarkPose(data, actor);
                if (not pose or not sameIndex3(pose->pos, hitPose->pos))
                    continue;
                if (const auto alias = actorAlias(context, actor))
                    out.push_back(*alias);
            }
            return out;
        }

        auto selectedCells(Reading context, const Store& store, const mech::Blueprint& data, const std::vector<QuarkActor>& actors) -> std::set<index3, CellPosLess> {
            std::set<index3, CellPosLess> cells;
            for (const auto alias : store.aliases) {
                const auto actor = findActorByAlias(context, actors, alias);
                if (not actor)
                    continue;
                if (const auto pose = quarkPose(data, *actor))
                    cells.insert(pose->pos);
            }
            return cells;
        }

    } // namespace

    auto sameIndex3(const index3& a, const index3& b) -> bool {
        return a.x == b.x and a.y == b.y and a.z == b.z;
    }

    auto cellOccupied(const mech::Blueprint& data, const index3& pos) -> bool {
        for (const auto& knot : data.knots) {
            if (sameIndex3(knot.pose.pos, pos))
                return true;
        }
        for (const auto& halfChord : data.halfChords) {
            if (sameIndex3(halfChord.pose.pos, pos))
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
        store.clipboard = mech::Blueprint{.name = "clipboard", .author = {}, .knots = {}, .halfChords = {}};
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
        return store.clipboard.knots.empty() and store.clipboard.halfChords.empty();
    }

    auto canPaste(const mech::Blueprint& target, const mech::Blueprint& clipboard) -> bool {
        if (clipboard.knots.empty() and clipboard.halfChords.empty())
            return false;
        std::set<index3, CellPosLess> cells;
        for (const auto& knot : clipboard.knots)
            cells.insert(knot.pose.pos);
        for (const auto& halfChord : clipboard.halfChords)
            cells.insert(halfChord.pose.pos);
        for (const auto& pos : cells) {
            if (cellOccupied(target, pos))
                return false;
        }
        return true;
    }

    auto clipboardCells(const mech::Blueprint& clipboard) -> std::set<index3, CellPosLess> {
        std::set<index3, CellPosLess> cells;
        for (const auto& knot : clipboard.knots)
            cells.insert(knot.pose.pos);
        for (const auto& halfChord : clipboard.halfChords)
            cells.insert(halfChord.pose.pos);
        return cells;
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
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, hovered).data;
        const auto cells = selectedCells(context, store, data, actors);
        if (cells.empty())
            return;
        for (const auto& actor : actors) {
            const auto pose = quarkPose(data, actor);
            if (not pose or not cells.contains(pose->pos))
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
        std::set<std::size_t> knots;
        std::set<std::size_t> halfChords;
        for (const auto alias : store.aliases) {
            const auto actor = findActorByAlias(context, actors, alias);
            if (not actor)
                continue;
            switch (actor->kind) {
                case QuarkActor::Kind::knot: knots.insert(actor->index); break;
                case QuarkActor::Kind::halfChord: halfChords.insert(actor->index); break;
            }
        }
        for (const auto index : knots) {
            if (index < data.knots.size())
                store.clipboard.knots.push_back(data.knots[index]);
        }
        for (const auto index : halfChords) {
            if (index < data.halfChords.size())
                store.clipboard.halfChords.push_back(data.halfChords[index]);
        }
    }

    auto eraseSelected(Writing context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors) -> bool {
        if (store.aliases.empty() or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return false;
        std::set<std::size_t> knots;
        std::set<std::size_t> halfChords;
        for (const auto alias : store.aliases) {
            const auto actor = findActorByAlias(context, actors, alias);
            if (not actor)
                continue;
            switch (actor->kind) {
                case QuarkActor::Kind::knot: knots.insert(actor->index); break;
                case QuarkActor::Kind::halfChord: halfChords.insert(actor->index); break;
            }
        }
        clear(store);
        if (knots.empty() and halfChords.empty())
            return false;
        auto data = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
        eraseDescending(data->data.knots, knots);
        eraseDescending(data->data.halfChords, halfChords);
        return true;
    }

    auto rotateSelected(Writing context, Store& store, resource::blueprint::Asset::Id hovered, const std::vector<QuarkActor>& actors, mech::space::orient::Semiaxis axis) -> bool {
        if (store.aliases.empty() or not with<::eltanin::resource::blueprint::Asset>::exists(context, hovered))
            return false;
        const auto delta = mech::space::orient::turn(axis)[0];
        const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(delta)];
        const auto& data = with<::eltanin::resource::blueprint::Asset>::get(context, hovered).data;
        const auto cells = selectedCells(context, store, data, actors);
        if (cells.empty())
            return false;
        const auto remap = remapCells(cells, axis);
        for (const auto& [from, to] : remap) {
            if (cells.contains(to))
                continue;
            if (cellOccupied(data, to))
                return false;
        }
        store.pendingRestore = selectionRefs(context, actors, store.aliases);
        {
            auto writable = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
            for (auto& knot : writable->data.knots) {
                if (not cells.contains(knot.pose.pos))
                    continue;
                knot.pose.pos = remap.at(knot.pose.pos);
                knot.pose.ori = composeRow[static_cast<std::size_t>(knot.pose.ori)];
            }
            for (auto& halfChord : writable->data.halfChords) {
                if (not cells.contains(halfChord.pose.pos))
                    continue;
                halfChord.pose.pos = remap.at(halfChord.pose.pos);
                halfChord.pose.ori = composeRow[static_cast<std::size_t>(halfChord.pose.ori)];
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
        const auto cells = selectedCells(context, store, data, actors);
        if (cells.empty())
            return false;
        for (const auto& from : cells) {
            const auto to = addIndex3(from, step);
            if (cells.contains(to))
                continue;
            if (cellOccupied(data, to))
                return false;
        }
        store.pendingRestore = selectionRefs(context, actors, store.aliases);
        {
            auto writable = with<::eltanin::resource::blueprint::Asset>::modify(context, hovered);
            for (auto& knot : writable->data.knots) {
                if (not cells.contains(knot.pose.pos))
                    continue;
                knot.pose.pos = addIndex3(knot.pose.pos, step);
            }
            for (auto& halfChord : writable->data.halfChords) {
                if (not cells.contains(halfChord.pose.pos))
                    continue;
                halfChord.pose.pos = addIndex3(halfChord.pose.pos, step);
            }
        }
        return true;
    }

    auto rotateClipboard(Store& store, mech::space::orient::Semiaxis axis) -> bool {
        if (clipboardEmpty(store))
            return false;
        const auto delta = mech::space::orient::turn(axis)[0];
        const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(delta)];
        const auto cells = clipboardCells(store.clipboard);
        if (cells.empty())
            return false;
        const auto remap = remapCells(cells, axis);
        for (auto& knot : store.clipboard.knots) {
            knot.pose.pos = remap.at(knot.pose.pos);
            knot.pose.ori = composeRow[static_cast<std::size_t>(knot.pose.ori)];
        }
        for (auto& halfChord : store.clipboard.halfChords) {
            halfChord.pose.pos = remap.at(halfChord.pose.pos);
            halfChord.pose.ori = composeRow[static_cast<std::size_t>(halfChord.pose.ori)];
        }
        return true;
    }

    auto moveClipboard(Store& store, index3 step) -> bool {
        if (clipboardEmpty(store) or (step.x == 0 and step.y == 0 and step.z == 0))
            return false;
        for (auto& knot : store.clipboard.knots)
            knot.pose.pos = addIndex3(knot.pose.pos, step);
        for (auto& halfChord : store.clipboard.halfChords)
            halfChord.pose.pos = addIndex3(halfChord.pose.pos, step);
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
        for (const auto& knot : store.clipboard.knots)
            writable->data.knots.push_back(knot);
        for (const auto& halfChord : store.clipboard.halfChords)
            writable->data.halfChords.push_back(halfChord);
        return true;
    }

    void handlePointer(Reading context, Store& store, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors, renderer::Integer32 under) {
        if (not ImGui::GetIO().WantCaptureMouse and under != renderer::Integer32{0}) {
            const auto hit = findActorByAlias(context, actors, under);
            const bool shift = ImGui::GetIO().KeyShift;
            const mech::Blueprint* blueprintData = (hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *hovered))
                ? &with<::eltanin::resource::blueprint::Asset>::get(context, *hovered).data
                : nullptr;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and hit) {
                if (shift and blueprintData) {
                    for (const auto alias : familyAliases(context, actors, *blueprintData, *hit))
                        addAlias(store.aliases, alias);
                } else {
                    addAlias(store.aliases, under);
                }
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) and hit) {
                if (shift and blueprintData) {
                    for (const auto alias : familyAliases(context, actors, *blueprintData, *hit))
                        removeAlias(store.aliases, alias);
                } else {
                    removeAlias(store.aliases, under);
                }
            }
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and not ImGui::GetIO().WantCaptureMouse and under == renderer::Integer32{0}) {
            clear(store);
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

    void drawPanel(Reading context, Store& store, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered, const std::vector<QuarkActor>& actors) {
        ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 220.0f}, ImGuiCond_FirstUseEver);
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
                ImGui::Separator();
                ImGui::TextDisabled("Del — remove · x/RMB deselect · Shift+LMB family · WASD/QE move · Shift+WASD/QE rotate");
                const auto* blueprintData = (hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *hovered))
                    ? &with<::eltanin::resource::blueprint::Asset>::get(context, *hovered).data
                    : nullptr;

                std::map<index3, std::vector<renderer::Integer32>, CellPosLess> byCell;
                std::vector<renderer::Integer32> orphans;
                for (const auto alias : store.aliases) {
                    const auto actor = findActorByAlias(context, actors, alias);
                    if (blueprintData and actor) {
                        if (const auto pose = quarkPose(*blueprintData, *actor)) {
                            byCell[pose->pos].push_back(alias);
                            continue;
                        }
                    }
                    orphans.push_back(alias);
                }

                const auto drawLeaf = [&](renderer::Integer32 alias) {
                    const auto actor = findActorByAlias(context, actors, alias);
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
    }

    auto drawClipboardPanel(Writing context, Store& store, ImVec2 blueprintsPos, ImVec2 blueprintsSize, base::maybe<resource::blueprint::Asset::Id> hovered) -> bool {
        ImGui::SetNextWindowPos(ImVec2{blueprintsPos.x + blueprintsSize.x + 8.0f, blueprintsPos.y + 228.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2{360.0f, 140.0f}, ImGuiCond_FirstUseEver);
        bool pasted = false;
        if (ImGui::Begin("Clipboard")) {
            ImGui::TextDisabled("%zu knots, %zu half-chords", store.clipboard.knots.size(), store.clipboard.halfChords.size());
            const bool empty = clipboardEmpty(store);
            bool allowed = false;
            if (not empty and hovered.exists() and with<::eltanin::resource::blueprint::Asset>::exists(context, *hovered))
                allowed = canPaste(with<::eltanin::resource::blueprint::Asset>::get(context, *hovered).data, store.clipboard);
            if (empty)
                ImGui::TextDisabled("empty");
            else if (allowed)
                ImGui::TextUnformatted("can paste");
            else
                ImGui::TextUnformatted("blocked");
            const char* focusLabel = store.focus == Focus::clipboard ? "focus: buffer (B)" : "focus: selection (B)";
            if (ImGui::Button(focusLabel) and not empty)
                toggleFocus(store);
            if (ImGui::Button("paste") and allowed and hovered.exists())
                pasted = pasteClipboard(context, store, *hovered);
            ImGui::SameLine();
            if (ImGui::Button("clear"))
                resetClipboard(store);
            ImGui::TextDisabled("B focus · WASD/QE move · Shift+WASD/QE rotate");
        }
        ImGui::End();
        return pasted;
    }

} // namespace eltanin::views::blueprints::selection
