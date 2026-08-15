#include "views/blueprints/history.h"

#include <utility>

namespace eltanin::views::blueprints::history {

    namespace {

        auto restoreDocument(Writing context, mech::Blueprint::Id id, const Blueprint& snapshot) -> void {
            auto writable = with<::eltanin::mech::Blueprint>::modify(context, id);
            const auto file = writable->file;
            *writable = snapshot;
            writable->file = file;
            with<::eltanin::mech::Blueprint>::save(context, id);
        }

        auto liveCopy(Reading context, mech::Blueprint::Id id) -> Blueprint {
            return with<::eltanin::mech::Blueprint>::get(context, id);
        }

    } // namespace

    void clear(Store& store) {
        store.bound.reset();
        store.undo.clear();
        store.redo.clear();
    }

    void bind(Store& store, base::maybe<mech::Blueprint::Id> id) {
        if (store.bound.has_value() and id.has_value() and *store.bound == *id)
            return;
        clear(store);
        store.bound = id;
    }

    void record(Store& store, mech::Blueprint::Id id, std::string label, const Blueprint& before) {
        if (not store.bound.has_value() or *store.bound != id)
            bind(store, id);
        store.redo.clear();
        store.undo.push_back(Step{.label = std::move(label), .document = before});
        while (store.undo.size() > depth)
            store.undo.pop_front();
    }

    auto canUndo(const Store& store) -> bool {
        return not store.undo.empty();
    }

    auto canRedo(const Store& store) -> bool {
        return not store.redo.empty();
    }

    auto undo(Writing context, Store& store, mech::Blueprint::Id id) -> bool {
        if (store.undo.empty() or not with<::eltanin::mech::Blueprint>::exists(context, id))
            return false;
        if (not store.bound.has_value() or *store.bound != id)
            return false;
        auto step = std::move(store.undo.back());
        store.undo.pop_back();
        store.redo.push_back(Step{.label = step.label, .document = liveCopy(context, id)});
        while (store.redo.size() > depth)
            store.redo.pop_front();
        restoreDocument(context, id, step.document);
        return true;
    }

    auto redo(Writing context, Store& store, mech::Blueprint::Id id) -> bool {
        if (store.redo.empty() or not with<::eltanin::mech::Blueprint>::exists(context, id))
            return false;
        if (not store.bound.has_value() or *store.bound != id)
            return false;
        auto step = std::move(store.redo.back());
        store.redo.pop_back();
        store.undo.push_back(Step{.label = step.label, .document = liveCopy(context, id)});
        while (store.undo.size() > depth)
            store.undo.pop_front();
        restoreDocument(context, id, step.document);
        return true;
    }

    auto drawWindow(Store& store) -> UiAction {
        auto action = UiAction::none;
        if (not ImGui::Begin("Actions")) {
            ImGui::End();
            return action;
        }
        ImGui::TextDisabled("snapshots %zu / %zu", store.undo.size(), depth);
        const bool undoOk = canUndo(store);
        const bool redoOk = canRedo(store);
        if (not undoOk)
            ImGui::BeginDisabled();
        if (ImGui::Button("Undo") and undoOk)
            action = UiAction::undo;
        if (not undoOk)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (not redoOk)
            ImGui::BeginDisabled();
        if (ImGui::Button("Redo") and redoOk)
            action = UiAction::redo;
        if (not redoOk)
            ImGui::EndDisabled();
        ImGui::TextDisabled("Ctrl+Z / Ctrl+Y");
        ImGui::Separator();
        ImGui::TextDisabled("Undo stack (oldest → newest)");
        if (store.undo.empty()) {
            ImGui::TextDisabled("(empty)");
        } else {
            for (std::size_t i = 0; i < store.undo.size(); ++i)
                ImGui::Text("%zu. %s", i + 1, store.undo[i].label.c_str());
        }
        if (not store.redo.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Redo stack (next first)");
            for (std::size_t i = 0; i < store.redo.size(); ++i) {
                const auto& step = store.redo[store.redo.size() - 1 - i];
                ImGui::TextDisabled("%zu. %s", i + 1, step.label.c_str());
            }
        }
        ImGui::End();
        return action;
    }

}
