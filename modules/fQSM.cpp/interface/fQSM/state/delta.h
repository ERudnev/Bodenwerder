#pragma once

#include <iterator>
#include <memory>
#include <stdexcept>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/state/slice/preview.h>
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
        using value_type = item::Delta<Meta>;

        enum class Layer {
            all,
            added,
            removed,
            updated,
        };

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = item::Delta<Meta>;

            Iterator(cref<StateSlice> state, cref<PatchSlice> patch, PatchIterator current, PatchIterator end, Layer layer = Layer::all)
                : state(state), patch(patch), current(current), end(end), layer(layer) {
                skip_to_match();
            }

            auto operator*() const -> value_type {
                return make_value(current);
            }

            Iterator& operator++() {
                ++current;
                skip_to_match();
                return *this;
            }

            Iterator operator++(int) {
                Iterator copy = *this;
                ++*this;
                return copy;
            }

            bool operator==(const Iterator& other) const { return current == other.current; }
            bool operator!=(const Iterator& other) const { return !(*this == other); }

        private:
            static auto matches(Layer layer, const value_type& change) -> bool {
                if (layer == Layer::all) return change.good();
                if (layer == Layer::added) return change.add();
                if (layer == Layer::removed) return change.remove();
                if (layer == Layer::updated) return change.update();
                return false;
            }

            void skip_to_match() {
                while (current != end) {
                    if (matches(layer, make_value(current))) return;
                    ++current;
                }
            }

            auto make_value(PatchIterator current) const -> value_type {
                const auto entry = *current;
                const auto* before = state->items().find(entry.first);
                const auto* after = entry.second.has_value() ? std::addressof(entry.second.value()) : nullptr;
                return value_type{entry.first, before, after};
            }

            cref<StateSlice> state;
            cref<PatchSlice> patch;
            PatchIterator current;
            PatchIterator end;
            Layer layer;
        };

        struct LayerView {
            cref<StateSlice> state;
            cref<PatchSlice> patch;
            Layer layer;

            auto begin() const -> Iterator {
                return Iterator{state, patch, patch->items().begin(), patch->items().end(), layer};
            }

            auto end() const -> Iterator {
                return Iterator{state, patch, patch->items().end(), patch->items().end(), layer};
            }
        };

        Delta(cref<StateSlice> state, cref<PatchSlice> patch) : state(state), patch(patch) {}
        explicit Delta(const Preview<Meta>& preview) : Delta(preview.state, preview.patch) {}

        auto begin() const -> Iterator { return Iterator{state, patch, patch->items().begin(), patch->items().end(), Layer::all}; }
        auto end() const -> Iterator { return Iterator{state, patch, patch->items().end(), patch->items().end(), Layer::all}; }

        auto added() const -> LayerView { return LayerView{state, patch, Layer::added}; }
        auto removed() const -> LayerView { return LayerView{state, patch, Layer::removed}; }
        auto updated() const -> LayerView { return LayerView{state, patch, Layer::updated}; }

    private:
        cref<StateSlice> state;
        cref<PatchSlice> patch;
    };
    
}