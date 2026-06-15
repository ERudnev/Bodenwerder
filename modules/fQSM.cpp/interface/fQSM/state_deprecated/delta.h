#pragma once

#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/state/slice/draft.h>
#include <fQSM/state/slice/view.h>

namespace fqsm::state::item {
    namespace axis = meta::axis;

    /* Loos as wrong abstraction. Consider no remove
    enum class ChangeType {
        addition,
        deletion,
        modification,
    };
    */

    template<aspect::Any Meta>
    struct Delta {
        using Id = fqsm::Id<Meta>;
        using Item = typename slice::View<Meta, axis::order::state>::Item;

        Id id;
        const Item* before;
        const Item* after;

        bool good() const { return before || after; }
        bool add() const { return !before && after; }
        bool update() const { return before && after; }
        bool remove() const { return before && !after; }

        /* Loos as wrong abstraction. Consider no remove
        auto kind() const -> ChangeType {
            if (add()) return ChangeType::addition;
            if (update()) return ChangeType::modification;
            if (remove()) return ChangeType::deletion;
            throw std::logic_error("state::item::Delta::kind(): invalid delta");
        }*/
    };
}

namespace fqsm::state::slice {
    namespace axis = meta::axis;

    template<aspect::Any Meta>
    struct Delta {
        using StateSlice = View<Meta, axis::order::state>;
        using PatchSlice = View<Meta, axis::order::patch>;
        using Id = typename item::Delta<Meta>::Id;
        using Item = typename item::Delta<Meta>::Item;
        using PatchItem = typename PatchSlice::Item;
        using PatchIterator = typename PatchSlice::ItemsView::ReadIterator;
        using StateIterator = typename StateSlice::ItemsView::ReadIterator;
        using value_type = item::Delta<Meta>;

        enum class Layer {
            all,
            added,
            addedOrUpdated,
            removed,
            updated,
        };

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = item::Delta<Meta>;

            Iterator(cref<StateSlice> state, cref<PatchSlice> patch, PatchIterator current, PatchIterator end, Layer layer = Layer::all)
                : state(state), patch(patch), patchCurrent(current), patchEnd(end), layer(layer) {
                skip_to_match();
            }

            Iterator(cref<StateSlice> state, cref<PatchSlice> patch, StateIterator current, StateIterator end, Layer layer, bool)
                : state(state), patch(patch), stateCurrent(current), stateEnd(end), layer(layer) {
                skip_to_match();
            }

            auto operator*() const -> value_type {
                return patch->tainted() ? make_state_value(*stateCurrent) : make_patch_value(*patchCurrent);
            }

            Iterator& operator++() {
                if (patch->tainted()) {
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
                if (patch->tainted()) return stateCurrent == other.stateCurrent;
                return patchCurrent == other.patchCurrent;
            }
            bool operator!=(const Iterator& other) const { return !(*this == other); }

        private:
            static auto matches(Layer layer, const value_type& change) -> bool {
                if (layer == Layer::all) return change.good();
                if (layer == Layer::added) return change.add();
                if (layer == Layer::addedOrUpdated) return change.add() || change.update();
                if (layer == Layer::removed) return change.remove();
                if (layer == Layer::updated) return change.update();
                return false;
            }

            void skip_to_match() {
                if (patch->tainted()) {
                    while (stateCurrent != stateEnd) {
                        if (matches(layer, make_state_value(*stateCurrent))) return;
                        ++*stateCurrent;
                    }
                    return;
                }

                while (patchCurrent != patchEnd) {
                    if (matches(layer, make_patch_value(*patchCurrent))) return;
                    ++*patchCurrent;
                }
            }

            auto make_patch_value(PatchIterator current) const -> value_type {
                const auto entry = *current;
                const auto* before = state->items().find(entry.first);
                const auto* after = entry.second.has_value() ? std::addressof(entry.second.value()) : nullptr;
                return value_type{entry.first, before, after};
            }

            auto make_state_value(StateIterator current) const -> value_type {
                const auto entry = *current;
                return value_type{entry.first, std::addressof(entry.second), std::addressof(entry.second)};
            }

            cref<StateSlice> state;
            cref<PatchSlice> patch;
            std::optional<PatchIterator> patchCurrent;
            std::optional<PatchIterator> patchEnd;
            std::optional<StateIterator> stateCurrent;
            std::optional<StateIterator> stateEnd;
            Layer layer;
        };

        struct LayerView {
            cref<StateSlice> state;
            cref<PatchSlice> patch;
            Layer layer;

            auto begin() const -> Iterator {
                if (patch->tainted()) {
                    return Iterator{state, patch, state->items().begin(), state->items().end(), layer, true};
                }
                return Iterator{state, patch, patch->items().begin(), patch->items().end(), layer};
            }

            auto end() const -> Iterator {
                if (patch->tainted()) {
                    return Iterator{state, patch, state->items().end(), state->items().end(), layer, true};
                }
                return Iterator{state, patch, patch->items().end(), patch->items().end(), layer};
            }
        };

        Delta(cref<StateSlice> state, cref<PatchSlice> patch) : state(state), patch(patch) {}
        explicit Delta(const Draft<Meta>& preview) : Delta(preview.state, preview.patch) {}

        auto begin() const -> Iterator {
            if (patch->tainted()) return Iterator{state, patch, state->items().begin(), state->items().end(), Layer::all, true};
            return Iterator{state, patch, patch->items().begin(), patch->items().end(), Layer::all};
        }
        auto end() const -> Iterator {
            if (patch->tainted()) return Iterator{state, patch, state->items().end(), state->items().end(), Layer::all, true};
            return Iterator{state, patch, patch->items().end(), patch->items().end(), Layer::all};
        }

        auto added() const -> LayerView { return LayerView{state, patch, Layer::added}; }
        auto addedOrUpdated() const -> LayerView { return LayerView{state, patch, Layer::addedOrUpdated}; }
        auto removed() const -> LayerView { return LayerView{state, patch, Layer::removed}; }
        auto updated() const -> LayerView { return LayerView{state, patch, Layer::updated}; }

    private:
        cref<StateSlice> state;
        cref<PatchSlice> patch;
    };

}