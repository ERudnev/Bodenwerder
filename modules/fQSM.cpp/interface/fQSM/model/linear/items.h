#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>

#include <fQSM/erased/line.h>
#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    // Typed view over one erased line. Read-only over any ReadLine; mutable (Direct, integration) over a Line.
    template<category::Any Meta>
    class Items {
    public:
        using Key = Id<Meta>;
        using Value = Quantum<Meta>;
        using SizeType = std::size_t;

        struct Entry {
            Key id;
            const Value& value;
        };

        struct EntryRef {
            Key id;
            Value& value;
        };

        template<typename View>
        struct ArrowProxy {
            View view;
            const View* operator->() const { return &view; }
        };

        class ConstCursor {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = Entry;
            using pointer = void;
            using reference = Entry;

            ConstCursor() = default;
            explicit ConstCursor(erased::Cursor inner) : inner(inner) {}

            Entry operator*() const {
                const auto entry = *inner;
                return Entry{Key{entry.id}, *static_cast<const Value*>(entry.value)};
            }
            ArrowProxy<Entry> operator->() const { return ArrowProxy<Entry>{**this}; }
            ConstCursor& operator++() { ++inner; return *this; }
            ConstCursor operator++(int) { auto copy = *this; ++inner; return copy; }
            bool operator==(const ConstCursor& other) const { return inner == other.inner; }

        private:
            erased::Cursor inner;
        };

        class MutableCursor {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = EntryRef;
            using pointer = void;
            using reference = EntryRef;

            MutableCursor() = default;
            MutableCursor(erased::Line* line, std::size_t index) : line(line), index(index) {}

            EntryRef operator*() const {
                return EntryRef{Key{line->id_at(index)}, *static_cast<Value*>(line->value_at(index))};
            }
            ArrowProxy<EntryRef> operator->() const { return ArrowProxy<EntryRef>{**this}; }
            MutableCursor& operator++() { ++index; return *this; }
            MutableCursor operator++(int) { auto copy = *this; ++index; return copy; }
            bool operator==(const MutableCursor& other) const { return line == other.line and index == other.index; }

        private:
            erased::Line* line = nullptr;
            std::size_t index = 0;
        };

        explicit Items(const erased::ReadLine& line) : reader(&line) {}
        explicit Items(erased::Line& line) : reader(&line), writer(&line) {}
        Items(const Items&) = delete;
        Items& operator=(const Items&) = delete;

        const erased::ReadLine& line() const { return *reader; }

        bool contains(const Key& id) const { return find(id) != nullptr; }
        bool empty() const { return size() == 0; }
        SizeType size() const { return writer ? writer->size() : reader->size(); }

        const Value* find(const Key& id) const {
            const void* found = writer ? writer->find(id.raw()) : reader->find(id.raw());
            return static_cast<const Value*>(found);
        }

        Value* find(const Key& id) {
            return static_cast<Value*>(mutable_line().find_mutable(id.raw()));
        }

        const Value& at(const Key& id) const {
            if (const auto* found = find(id)) return *found;
            throw std::out_of_range("fqsm::model::linear::Items::at");
        }

        Value& at(const Key& id) {
            if (auto* found = find(id)) return *found;
            throw std::out_of_range("fqsm::model::linear::Items::at");
        }

        std::optional<Value> get(const Key& id) const {
            if (const auto* found = find(id)) return *found;
            return std::nullopt;
        }

        ConstCursor begin() const { return ConstCursor{reader->cursor_begin()}; }
        ConstCursor end() const { return ConstCursor{reader->cursor_end()}; }
        MutableCursor begin() { return MutableCursor{&mutable_line(), 0}; }
        MutableCursor end() { auto& line = mutable_line(); return MutableCursor{&line, line.size()}; }

        Value& insert(const Key& id, const Value& value) {
            return *static_cast<Value*>(mutable_line().insert(id.raw(), &value));
        }

        Value& insert(const Key& id, Value&& value) {
            return *static_cast<Value*>(mutable_line().emplace_move(id.raw(), &value));
        }

        bool erase(const Key& id) { return mutable_line().erase(id.raw()); }
        void clear() { mutable_line().clear(); }
        void reserve(SizeType capacity) { mutable_line().reserve(capacity); }

    private:
        erased::Line& mutable_line() const {
            if (not writer) throw std::logic_error("fqsm::model::linear::Items: read-only view");
            return *writer;
        }

        const erased::ReadLine* reader;
        erased::Line* writer = nullptr;
    };
}
