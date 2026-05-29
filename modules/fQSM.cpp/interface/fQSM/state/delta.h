#pragma once

#include <iterator>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/meta/alias.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/slice.h>

namespace fqsm::state::item {
    namespace axis = meta::axis;

    template<aspect::Any Meta>
    struct Delta {
        using Id = fqsm::Id<Meta>;
        using Item = typename slice::View<Meta, axis::order::state>::Item;

        Id id;
        const Item* before;
        const Item* after;        

        bool add() const { return !before && after; }
        bool update() const { return before && after; }
        bool remove() const { return before && !after; }
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
    
        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = item::Delta<Meta>;

            Iterator(const Delta& delta, PatchIterator current, PatchIterator end) : delta(&delta), current(current), end(end) { skip_to_change(); }

            auto operator*() const -> value_type {
                const auto entry = *current;
                const auto* before = delta->state->items().find(entry.first);
                const auto* after = entry.second.has_value() ? std::addressof(entry.second.value()) : nullptr;
                return value_type{entry.first, before, after};
            }

            Iterator& operator++() {
                ++current;
                skip_to_change();
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
            void skip_to_change() {
                while (current != end) {
                    const auto entry = *current;
                    if (entry.second.has_value() || delta->state->items().contains(entry.first)) return;
                    ++current;
                }
            }

            const Delta* delta;
            PatchIterator current;
            PatchIterator end;
        };
    
        Delta(cref<StateSlice> state, cref<PatchSlice> patch) : state(state), patch(patch) {}
        explicit Delta(const Overlay<Meta>& overlay) : state(overlay.state), patch(overlay.patch) {}

        auto begin() const -> Iterator { return Iterator{*this, patch->items().begin(), patch->items().end()}; }
        auto end() const -> Iterator { return Iterator{*this, patch->items().end(), patch->items().end()}; }

    private:
        cref<StateSlice> state;
        cref<PatchSlice> patch;
    };
    
}