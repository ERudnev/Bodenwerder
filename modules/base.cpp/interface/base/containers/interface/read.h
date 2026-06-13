#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

namespace base::table {

template<typename Key, typename Val>
class Read {
public:
    using KeyType = Key;
    using MappedType = Val;

    struct EntryView {
        const Key& first;
        const Val& second;
    };

    class ReadIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = EntryView;
        using pointer = const EntryView*;
        using reference = const EntryView&;

        ReadIterator(const ReadIterator& other)
            : state(other.state ? other.state->clone() : nullptr)
        {}

        ReadIterator(ReadIterator&&) noexcept = default;

        ReadIterator& operator=(const ReadIterator& other) {
            if (this == std::addressof(other)) return *this;
            state = other.state ? other.state->clone() : nullptr;
            return *this;
        }

        ReadIterator& operator=(ReadIterator&&) noexcept = default;

        EntryView operator*() const {
            return state->dereference();
        }

        struct ArrowProxy {
            EntryView view;
            const EntryView* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{state->dereference()};
        }

        ReadIterator& operator++() {
            state->increment();
            return *this;
        }

        ReadIterator operator++(int) {
            ReadIterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const ReadIterator& other) const {
            if (!state || !other.state) return state == other.state;
            return state->equals(*other.state);
        }

        bool operator!=(const ReadIterator& other) const {
            return !(*this == other);
        }

    private:
        friend class Read;

        struct State {
            virtual ~State() = default;
            virtual EntryView dereference() const = 0;
            virtual void increment() = 0;
            virtual bool equals(const State& other) const = 0;
            virtual std::unique_ptr<State> clone() const = 0;
        };

        template<typename Iterator>
        struct IteratorState final : State {
            explicit IteratorState(Iterator iterator)
                : iterator(std::move(iterator))
            {}

            EntryView dereference() const override {
                const auto entry = *iterator;
                return EntryView{entry.first, entry.second};
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

            Iterator iterator;
        };

        template<typename Iterator>
        explicit ReadIterator(Iterator iterator)
            : state(std::make_unique<IteratorState<Iterator>>(std::move(iterator)))
        {}

        std::unique_ptr<State> state;
    };

    virtual ~Read() = default;

    virtual bool contains(const Key& key) const = 0;
    virtual const Val* find(const Key& key) const = 0;
    virtual const Val& at(const Key& key) const = 0;
    virtual std::size_t size() const = 0;

    bool empty() const { return size() == 0; }

    std::optional<Val> get(const Key& key) const {
        if (const auto* found = find(key)) return *found;
        return std::nullopt;
    }

    ReadIterator begin() const {
        return read_begin();
    }

    ReadIterator end() const {
        return read_end();
    }

protected:
    template<typename Iterator>
    ReadIterator make_read_iterator(Iterator iterator) const {
        return ReadIterator(std::move(iterator));
    }

    virtual ReadIterator read_begin() const = 0;
    virtual ReadIterator read_end() const = 0;
};

} // namespace base::table
