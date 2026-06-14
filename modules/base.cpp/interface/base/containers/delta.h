#pragma once

#include <iterator>
#include <utility>

#include <base/containers/interface/read.h>
#include <base/containers/patch.h>

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
    using value_type = Change<Key, Val>;

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
        using value_type = Change<Key, Val>;

        Iterator(const View& state, const PatchView& patch, PatchIterator current, PatchIterator end, Layer layer = Layer::all)
            : state(std::addressof(state))
            , patch(std::addressof(patch))
            , current(std::move(current))
            , end(std::move(end))
            , layer(layer)
        {
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

        bool operator==(const Iterator& other) const {
            return state == other.state && patch == other.patch && current == other.current;
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
            while (current != end) {
                if (matches(layer, make_value(current))) return;
                ++current;
            }
        }

        auto make_value(PatchIterator iterator) const -> value_type {
            const auto entry = *iterator;
            const auto* before = state->find(entry.first);
            const auto* after = entry.second.has_value() ? std::addressof(entry.second.value()) : nullptr;
            return value_type{entry.first, before, after};
        }

        const View* state;
        const PatchView* patch;
        PatchIterator current;
        PatchIterator end;
        Layer layer;
    };

    struct LayerView {
        const View* state;
        const PatchView* patch;
        Layer layer;

        auto begin() const -> Iterator {
            return Iterator{*state, *patch, patch->begin(), patch->end(), layer};
        }

        auto end() const -> Iterator {
            return Iterator{*state, *patch, patch->end(), patch->end(), layer};
        }
    };

    Delta(const View& state, const PatchView& patch)
        : state(std::addressof(state))
        , patch(std::addressof(patch))
    {}

    auto begin() const -> Iterator {
        return Iterator{*state, *patch, patch->begin(), patch->end(), Layer::all};
    }

    auto end() const -> Iterator {
        return Iterator{*state, *patch, patch->end(), patch->end(), Layer::all};
    }

    auto added() const -> LayerView { return LayerView{state, patch, Layer::added}; }
    auto addedOrUpdated() const -> LayerView { return LayerView{state, patch, Layer::addedOrUpdated}; }
    auto removed() const -> LayerView { return LayerView{state, patch, Layer::removed}; }
    auto updated() const -> LayerView { return LayerView{state, patch, Layer::updated}; }

private:
    const View* state;
    const PatchView* patch;
};

} // namespace base
