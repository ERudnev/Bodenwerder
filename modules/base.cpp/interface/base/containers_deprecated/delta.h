#pragma once

#include <iterator>
#include <optional>
#include <utility>

#include <base/containers_deprecated/interface/read.h>
#include <base/containers_deprecated/patch.h>

namespace base {

template<typename Key, typename Val>
struct Change {
    const Key& key;
    const Val* before; // TODO: implement "Taint mode" as std::optional<const Val*>
    const Val* after;

    bool good() const { return before || after; }
    bool add() const { return !before && after; }
    bool update() const { return before && after; }
    bool remove() const { return before && !after; }
};

template<typename Key, typename Val>
class Delta {
public:
    using View = table::Read<Key, Val>;
    using PatchElement = patch::Element<Val>;
    using PatchView = table::Read<Key, PatchElement>;
    using PatchIterator = typename PatchView::ReadIterator;
    using StateIterator = typename View::ReadIterator;
    using value_type = Change<Key, Val>;

    enum class Layer {
        all,
        added,
        addedOrUpdated,
        removed,
        updated,
    };

    enum class StateInterpretation {
        clean,
        dirty,
    };

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Change<Key, Val>;

        Iterator(
            const View& state,
            const PatchView& patch,
            PatchIterator current,
            PatchIterator end,
            Layer layer,
            StateInterpretation interpretation)
            : state(std::addressof(state))
            , patch(std::addressof(patch))
            , patchCurrent(std::move(current))
            , patchEnd(std::move(end))
            , layer(layer)
            , interpretation(interpretation)
        {
            skip_to_match();
        }

        Iterator(
            const View& state,
            const PatchView& patch,
            StateIterator current,
            StateIterator end,
            Layer layer,
            StateInterpretation interpretation,
            bool)
            : state(std::addressof(state))
            , patch(std::addressof(patch))
            , patchCurrent(patch.begin())
            , patchEnd(patch.end())
            , stateCurrent(std::move(current))
            , stateEnd(std::move(end))
            , layer(layer)
            , interpretation(interpretation)
        {
            skip_to_match();
        }

        auto operator*() const -> value_type {
            if (interpretation == StateInterpretation::clean) {
                return make_patch_value(*patchCurrent);
            }
            if (stateCurrent != stateEnd) {
                return make_dirty_state_value(*stateCurrent);
            }
            return make_patch_only_value(*patchCurrent);
        }

        Iterator& operator++() {
            if (interpretation == StateInterpretation::clean) {
                ++*patchCurrent;
            } else if (stateCurrent != stateEnd) {
                ++*stateCurrent;
            } else {
                ++*patchCurrent;
            }
            skip_to_match();
            return *this;
        }

        Iterator operator++(int) {
            Iterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const Iterator& other) const {
            if (state != other.state || patch != other.patch || interpretation != other.interpretation) return false;
            if (interpretation == StateInterpretation::clean) return patchCurrent == other.patchCurrent;
            return stateCurrent == other.stateCurrent && patchCurrent == other.patchCurrent;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        static bool matches(Layer layer, const value_type& change) {
            if (layer == Layer::all) return change.good();
            if (layer == Layer::added) return change.add();
            if (layer == Layer::addedOrUpdated) return change.add() || change.update();
            if (layer == Layer::removed) return change.remove();
            if (layer == Layer::updated) return change.update();
            return false;
        }

        void skip_to_match() {
            if (interpretation == StateInterpretation::clean) {
                while (patchCurrent != patchEnd) {
                    if (matches(layer, make_patch_value(*patchCurrent))) return;
                    ++*patchCurrent;
                }
                return;
            }

            while (stateCurrent != stateEnd) {
                if (matches(layer, make_dirty_state_value(*stateCurrent))) return;
                ++*stateCurrent;
            }

            while (patchCurrent != patchEnd) {
                if (state->contains((*patchCurrent)->first)) {
                    ++*patchCurrent;
                    continue;
                }
                if (matches(layer, make_patch_only_value(*patchCurrent))) return;
                ++*patchCurrent;
            }
        }

        auto make_patch_value(PatchIterator iterator) const -> value_type {
            const auto entry = *iterator;
            const auto* before = state->find(entry.first);
            const auto* after = entry.second.has_value() ? std::addressof(entry.second.value()) : nullptr;
            return value_type{entry.first, before, after};
        }

        auto make_dirty_state_value(StateIterator iterator) const -> value_type {
            const auto entry = *iterator;
            if (const auto* patchEntry = patch->find(entry.first)) {
                const auto* after = patchEntry->has_value() ? std::addressof(patchEntry->value()) : nullptr;
                return value_type{entry.first, std::addressof(entry.second), after};
            }
            return value_type{entry.first, std::addressof(entry.second), std::addressof(entry.second)};
        }

        auto make_patch_only_value(PatchIterator iterator) const -> value_type {
            return make_patch_value(iterator);
        }

        const View* state;
        const PatchView* patch;
        std::optional<PatchIterator> patchCurrent;
        std::optional<PatchIterator> patchEnd;
        std::optional<StateIterator> stateCurrent;
        std::optional<StateIterator> stateEnd;
        Layer layer;
        StateInterpretation interpretation;
    };

    struct LayerView {
        const View& state;
        const PatchView& patch;
        const Layer layer;
        const StateInterpretation interpretation;

        auto begin() const -> Iterator {
            if (interpretation == StateInterpretation::clean) {
                return Iterator{state, patch, patch.begin(), patch.end(), layer, interpretation};
            }
            return Iterator{state, patch, state.begin(), state.end(), layer, interpretation, true};
        }

        auto end() const -> Iterator {
            if (interpretation == StateInterpretation::clean) {
                return Iterator{state, patch, patch.end(), patch.end(), layer, interpretation};
            }
            return Iterator{state, patch, state.end(), state.end(), layer, interpretation, true};
        }
    };

    Delta(const View& state, const PatchView& patch, StateInterpretation interpretation)
        : state(state)
        , patch(patch)
        , interpretation(interpretation)
    {}

    auto begin() const -> Iterator {
        if (interpretation == StateInterpretation::clean) {
            return Iterator{state, patch, patch.begin(), patch.end(), Layer::all, interpretation};
        }
        return Iterator{state, patch, state.begin(), state.end(), Layer::all, interpretation, true};
    }

    auto end() const -> Iterator {
        if (interpretation == StateInterpretation::clean) {
            return Iterator{state, patch, patch.end(), patch.end(), Layer::all, interpretation};
        }
        return Iterator{state, patch, state.end(), state.end(), Layer::all, interpretation, true};
    }

    auto added() const -> LayerView { return LayerView{state, patch, Layer::added, interpretation}; }
    auto addedOrUpdated() const -> LayerView { return LayerView{state, patch, Layer::addedOrUpdated, interpretation}; }
    auto removed() const -> LayerView { return LayerView{state, patch, Layer::removed, interpretation}; }
    auto updated() const -> LayerView { return LayerView{state, patch, Layer::updated, interpretation}; }

private:
    const View& state;
    const PatchView& patch;
    const StateInterpretation interpretation;
};

} // namespace base
