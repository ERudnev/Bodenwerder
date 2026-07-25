#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <base/cannonball/delta/changesLanguage.h>

namespace base::cannonball::delta {

template<typename Key, typename Val>
class Interface {
public:
    using KeyType = Key;
    using MappedType = Val;
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
        using pointer = const value_type*;
        using reference = const value_type&;

        Iterator(const Iterator& other)
            : state(other.state ? other.state->clone() : nullptr)
        {}

        Iterator(Iterator&&) noexcept = default;

        Iterator& operator=(const Iterator& other) {
            if (this == std::addressof(other)) return *this;
            state = other.state ? other.state->clone() : nullptr;
            return *this;
        }

        Iterator& operator=(Iterator&&) noexcept = default;

        value_type operator*() const {
            return state->dereference();
        }

        struct ArrowProxy {
            value_type view;
            const value_type* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{state->dereference()};
        }

        Iterator& operator++() {
            state->increment();
            return *this;
        }

        Iterator operator++(int) {
            Iterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const Iterator& other) const {
            if (!state || !other.state) return state == other.state;
            return state->equals(*other.state);
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        friend class Interface;

        struct State {
            virtual ~State() = default;
            virtual value_type dereference() const = 0;
            virtual void increment() = 0;
            virtual bool equals(const State& other) const = 0;
            virtual std::unique_ptr<State> clone() const = 0;
        };

        template<typename IteratorImpl>
        struct IteratorState final : State {
            explicit IteratorState(IteratorImpl iterator)
                : iterator(std::move(iterator))
            {}

            value_type dereference() const override {
                return *iterator;
            }

            void increment() override {
                ++iterator;
            }

            bool equals(const State& other) const override {
                const auto* typed = dynamic_cast<const IteratorState*>(&other);
                return typed && iterator == typed->iterator;
            }

            std::unique_ptr<State> clone() const override {
                return std::make_unique<IteratorState>(iterator);
            }

            IteratorImpl iterator;
        };

        template<typename IteratorImpl>
        explicit Iterator(IteratorImpl iterator)
            : state(std::make_unique<IteratorState<IteratorImpl>>(std::move(iterator)))
        {}

        std::unique_ptr<State> state;
    };

    // Narrow layer range: *it is Appeared / Updated / Gone / Upserted (not Change).
    template<detail::NarrowKind Kind>
    struct NarrowLayerView {
        using value_type = detail::narrow_result_t<Kind, Key, Val>;

        const Interface* owner;
        Layer layer;

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = detail::narrow_result_t<Kind, Key, Val>;
            using pointer = const value_type*;
            using reference = const value_type&;

            Iterator() = default;
            explicit Iterator(Interface::Iterator inner) : inner(std::move(inner)) {}

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
            Interface::Iterator inner;
        };

        auto begin() const -> Iterator {
            return Iterator{owner->delta_begin(layer)};
        }

        auto end() const -> Iterator {
            return Iterator{owner->delta_end(layer)};
        }

        auto size() const -> std::size_t {
            return static_cast<std::size_t>(std::distance(begin(), end()));
        }
    };

    struct AllView {
        const Interface* owner;

        auto begin() const -> Iterator { return owner->delta_begin(Layer::all); }
        auto end() const -> Iterator { return owner->delta_end(Layer::all); }
        auto size() const -> std::size_t {
            return static_cast<std::size_t>(std::distance(begin(), end()));
        }
    };

    using AppearedView = NarrowLayerView<detail::NarrowKind::appeared>;
    using UpdatedView = NarrowLayerView<detail::NarrowKind::updated>;
    using GoneView = NarrowLayerView<detail::NarrowKind::gone>;
    using UpsertedView = NarrowLayerView<detail::NarrowKind::upserted>;

    virtual ~Interface() = default;

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

protected:
    template<typename IteratorImpl>
    auto make_delta_iterator(IteratorImpl iterator) const -> Iterator {
        return Iterator(std::move(iterator));
    }

    virtual auto delta_begin(Layer layer) const -> Iterator = 0;
    virtual auto delta_end(Layer layer) const -> Iterator = 0;
};

} // namespace base::cannonball::delta
