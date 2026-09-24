#pragma once

#include <cstddef>
#include <iterator>
#include <utility>

#include <base/cannonball/delta/changesLanguage.h>
#include <base/cannonball/delta/cursor.h>
#include <base/cannonball/patchlet.h>
#include <base/cannonball/table/read.h>

namespace base::cannonball::delta {

template<typename Key, typename Val>
class Delta {
public:
    using KeyType = Key;
    using MappedType = Val;
    using value_type = Change<Key, Val>;
    using View = table::Read<Key, Val>;
    using PatchView = table::Read<Key, Patchlet<Val>>;
    using Iterator = DeltaCursor<Key, Val>;

    // Narrow layer range: *it is Appeared / Updated / Gone / Upserted (not Change).
    template<detail::NarrowKind Kind>
    struct NarrowLayerView {
        using value_type = detail::narrow_result_t<Kind, Key, Val>;

        const Delta* owner;
        Layer layer;

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = detail::narrow_result_t<Kind, Key, Val>;
            using pointer = const value_type*;
            using reference = const value_type&;

            Iterator() = default;
            explicit Iterator(Delta::Iterator inner) : inner(std::move(inner)) {}

            value_type operator*() const {
                return detail::project_as<Kind, Key, Val>(*inner);
            }

            struct ArrowProxy {
                value_type view;
                const value_type* operator->() const { return &view; }
            };

            ArrowProxy operator->() const {
                return ArrowProxy{detail::project_as<Kind, Key, Val>(*inner)};
            }

            Iterator& operator++() {
                ++inner;
                return *this;
            }

            Iterator operator++(int) {
                Iterator copy = *this;
                ++*this;
                return copy;
            }

            bool operator==(const Iterator& other) const { return inner == other.inner; }
            bool operator!=(const Iterator& other) const { return inner != other.inner; }

        private:
            Delta::Iterator inner;
        };

        auto begin() const -> Iterator {
            return Iterator{owner->delta_begin(layer)};
        }

        auto end() const -> Iterator {
            return Iterator{owner->delta_end(layer)};
        }

        auto empty() const -> bool {
            return owner->layer_empty(layer);
        }

        auto size() const -> std::size_t {
            return static_cast<std::size_t>(std::distance(begin(), end()));
        }
    };

    struct AllView {
        const Delta* owner;

        auto begin() const -> Delta::Iterator { return owner->delta_begin(Layer::all); }
        auto end() const -> Delta::Iterator { return owner->delta_end(Layer::all); }
        auto empty() const -> bool { return owner->layer_empty(Layer::all); }
        auto size() const -> std::size_t {
            return static_cast<std::size_t>(std::distance(begin(), end()));
        }
    };

    using AppearedView = NarrowLayerView<detail::NarrowKind::appeared>;
    using UpdatedView = NarrowLayerView<detail::NarrowKind::updated>;
    using GoneView = NarrowLayerView<detail::NarrowKind::gone>;
    using UpsertedView = NarrowLayerView<detail::NarrowKind::upserted>;

    Delta(const View& state, const PatchView& patch, Mode mode)
        : state(state)
        , patch(patch)
        , mode(mode)
    {}

    auto begin() const -> Iterator {
        return delta_begin(Layer::all);
    }

    auto end() const -> Iterator {
        return delta_end(Layer::all);
    }

    auto all() const -> AllView { return AllView{this}; }
    auto added() const -> AppearedView { return AppearedView{this, Layer::added}; }
    auto addedOrUpdated() const -> UpsertedView { return UpsertedView{this, Layer::addedOrUpdated}; }
    auto removed() const -> GoneView { return GoneView{this, Layer::removed}; }
    auto updated() const -> UpdatedView { return UpdatedView{this, Layer::updated}; }

    auto empty() const -> bool { return layer_empty(Layer::all); }
    auto size() const -> std::size_t {
        return static_cast<std::size_t>(std::distance(begin(), end()));
    }

private:
    auto layer_empty(Layer layer) const -> bool {
        return delta_begin(layer) == delta_end(layer);
    }

    auto delta_begin(Layer layer) const -> Iterator {
        if (mode == Mode::clean)
            return Iterator::clean_at(state, patch, patch.begin(), patch.end(), layer);
        return Iterator::dirty_at(state, patch, state.begin(), state.end(), patch.begin(), patch.end(), layer);
    }

    auto delta_end(Layer layer) const -> Iterator {
        if (mode == Mode::clean)
            return Iterator::clean_at(state, patch, patch.end(), patch.end(), layer);
        return Iterator::dirty_at(state, patch, state.end(), state.end(), patch.end(), patch.end(), layer);
    }

    const View& state;
    const PatchView& patch;
    const Mode mode;
};

} // namespace base::cannonball::delta
