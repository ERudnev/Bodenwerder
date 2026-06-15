#pragma once

#include <iterator>
#include <memory>
#include <optional>
#include <utility>

namespace base::cannonball::delta {

template<typename Key, typename Val>
struct Change {
    const Key& key;
    std::optional<const Val*> before; // TODO: implement "Taint mode" as std::optional<const Val*>
    const Val* after;

    bool good() const { return before.has_value() || after; }
    bool tainted() const { return !before.has_value() && after; }
    bool add() const { return before.has_value() && !before.value() && after; }
    bool update() const { return before.has_value() && before.value() && after; }
    bool addedOrUpdated() const { return add() || update() || tainted(); }
    bool remove() const { return before.has_value() && before.value() && !after; }
};

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

    struct LayerView {
        const Interface* owner;
        const Layer layer;

        auto begin() const -> Iterator {
            return owner->delta_begin(layer);
        }

        auto end() const -> Iterator {
            return owner->delta_end(layer);
        }
    };

    virtual ~Interface() = default;

    auto begin() const -> Iterator {
        return delta_begin(Layer::all);
    }

    auto end() const -> Iterator {
        return delta_end(Layer::all);
    }

    auto added() const -> LayerView { return LayerView{this, Layer::added}; }
    auto addedOrUpdated() const -> LayerView { return LayerView{this, Layer::addedOrUpdated}; }
    auto removed() const -> LayerView { return LayerView{this, Layer::removed}; }
    auto updated() const -> LayerView { return LayerView{this, Layer::updated}; }

protected:
    template<typename IteratorImpl>
    auto make_delta_iterator(IteratorImpl iterator) const -> Iterator {
        return Iterator(std::move(iterator));
    }

    virtual auto delta_begin(Layer layer) const -> Iterator = 0;
    virtual auto delta_end(Layer layer) const -> Iterator = 0;
};

} // namespace base::cannonball::delta