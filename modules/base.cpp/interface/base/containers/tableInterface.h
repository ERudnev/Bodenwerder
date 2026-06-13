#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

#include <base/containers/tableView.h>

namespace base {

template<typename Key, typename Val>
class TableInterface : public TableView<Key, Val> {
public:
    using View = TableView<Key, Val>;
    using KeyType = typename View::KeyType;
    using MappedType = typename View::MappedType;
    using SizeType = std::size_t;

    using EntryView = typename View::EntryView;
    using ReadIterator = typename View::ReadIterator;

    struct EntryRef {
        const Key& first;
        Val& second;
    };

    class WriteIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = EntryRef;
        using pointer = EntryRef*;
        using reference = EntryRef&;

        WriteIterator(const WriteIterator& other)
            : state(other.state ? other.state->clone() : nullptr)
        {}

        WriteIterator(WriteIterator&&) noexcept = default;

        WriteIterator& operator=(const WriteIterator& other) {
            if (this == std::addressof(other)) return *this;
            state = other.state ? other.state->clone() : nullptr;
            return *this;
        }

        WriteIterator& operator=(WriteIterator&&) noexcept = default;

        EntryRef operator*() const {
            return state->dereference();
        }

        struct ArrowProxy {
            EntryRef view;
            const EntryRef* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{state->dereference()};
        }

        WriteIterator& operator++() {
            state->increment();
            return *this;
        }

        WriteIterator operator++(int) {
            WriteIterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const WriteIterator& other) const {
            if (!state || !other.state) return state == other.state;
            return state->equals(*other.state);
        }

        bool operator!=(const WriteIterator& other) const {
            return !(*this == other);
        }

    private:
        friend class TableInterface;

        struct State {
            virtual ~State() = default;
            virtual EntryRef dereference() const = 0;
            virtual void increment() = 0;
            virtual bool equals(const State& other) const = 0;
            virtual std::unique_ptr<State> clone() const = 0;
        };

        template<typename Iterator>
        struct IteratorState final : State {
            explicit IteratorState(Iterator iterator)
                : iterator(std::move(iterator))
            {}

            EntryRef dereference() const override {
                auto entry = *iterator;
                return EntryRef{entry.first, entry.second};
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
        explicit WriteIterator(Iterator iterator)
            : state(std::make_unique<IteratorState<Iterator>>(std::move(iterator)))
        {}

        std::unique_ptr<State> state;
    };

    virtual ~TableInterface() = default;

    using View::at;
    using View::contains;
    using View::find;
    using View::get;
    using View::size;
    using View::begin;
    using View::end;

    virtual Val* find(const Key& key) = 0;
    virtual Val& at(const Key& key) = 0;

    bool empty() const { return this->size() == 0; }

    WriteIterator begin() {
        return write_begin();
    }

    WriteIterator end() {
        return write_end();
    }

    virtual void clear() = 0;
    virtual void reserve(SizeType capacity) = 0;
    virtual void insert(const Key& key, const Val& value) = 0;
    virtual void insert(Key&& key, Val&& value) = 0;
    virtual bool erase(const Key& key) = 0;

protected:
    template<typename Iterator>
    WriteIterator make_write_iterator(Iterator iterator) {
        return WriteIterator(std::move(iterator));
    }

    virtual WriteIterator write_begin() = 0;
    virtual WriteIterator write_end() = 0;
};

} // namespace base
