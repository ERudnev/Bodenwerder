#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

#include <base/containers/interface/write.h>

namespace base::table {

template<typename Key, typename Val>
class Access : public Write<Key, Val> {
public:
    using Interface = Write<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    using EntryView = typename Interface::EntryView;
    using ReadIterator = typename Interface::ReadIterator;

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
        friend class Access;

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

    virtual ~Access() = default;

    using Interface::at;
    using Interface::begin;
    using Interface::end;
    using Interface::find;

    virtual Val* find(const Key& key) = 0;
    virtual Val& at(const Key& key) = 0;

    WriteIterator begin() {
        return write_begin();
    }

    WriteIterator end() {
        return write_end();
    }

protected:
    template<typename Iterator>
    WriteIterator make_write_iterator(Iterator iterator) {
        return WriteIterator(std::move(iterator));
    }

    virtual WriteIterator write_begin() = 0;
    virtual WriteIterator write_end() = 0;
};

} // namespace base::table
