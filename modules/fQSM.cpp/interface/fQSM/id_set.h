#pragma once

// IdSet: a set of ids in one flat array. The quantum of a Group aspect.
// Open addressing with linear probing over the raw id values; a raw value of zero marks an empty slot,
// so the id with raw value zero is kept apart in a flag. Erase shifts the following entries back
// (no tombstones). A copy is one allocation and one memcpy, so a group can be a value quantum
// that the patch copies on change without a cost per element.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

#include <fQSM/identifier.h>

namespace fqsm {

    template<typename Id>
    class IdSet {
    public:
        using value_type = Id;
        using size_type = std::size_t;

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = Id;
            using pointer = void;
            using reference = Id;

            iterator() = default;
            iterator(const IdSet* set, std::size_t position) : set(set), position(position) { settle(); }

            Id operator*() const { return Id{position < set->table.size() ? set->table[position] : RawId{0}}; }
            iterator& operator++() { ++position; settle(); return *this; }
            iterator operator++(int) { auto copy = *this; ++*this; return copy; }
            bool operator==(const iterator& other) const { return position == other.position; }

        private:
            // valid positions: a used slot of the table, or table.size() when the zero id is present
            void settle() {
                const std::size_t slots = set->table.size();
                while (position < slots and set->table[position] == 0) ++position;
                if (position == slots and not set->hasZero) position = slots + 1;
            }

            const IdSet* set = nullptr;
            std::size_t position = 0;
        };
        using const_iterator = iterator;

        IdSet() = default;
        IdSet(std::initializer_list<Id> ids) { for (const auto id : ids) insert(id); }

        size_type size() const { return used + (hasZero ? 1 : 0); }
        bool empty() const { return size() == 0; }

        iterator begin() const { return iterator(this, 0); }
        iterator end() const { return iterator(this, table.size() + 1); }

        bool contains(const Id& id) const {
            const RawId raw = id.raw();
            if (raw == 0) return hasZero;
            return slot_of(raw) != npos;
        }
        size_type count(const Id& id) const { return contains(id) ? 1 : 0; }
        iterator find(const Id& id) const {
            const RawId raw = id.raw();
            if (raw == 0) return hasZero ? iterator(this, table.size()) : end();
            const std::size_t slot = slot_of(raw);
            return slot == npos ? end() : iterator(this, slot);
        }

        std::pair<iterator, bool> insert(const Id& id) {
            const RawId raw = id.raw();
            if (raw == 0) {
                const bool fresh = not hasZero;
                hasZero = true;
                return {iterator(this, table.size()), fresh};
            }
            if ((used + 1) * 2 > table.size()) grow();
            std::size_t slot = home(raw);
            while (table[slot] != 0) {
                if (table[slot] == raw) return {iterator(this, slot), false};
                slot = (slot + 1) & (table.size() - 1);
            }
            table[slot] = raw;
            ++used;
            return {iterator(this, slot), true};
        }

        size_type erase(const Id& id) {
            const RawId raw = id.raw();
            if (raw == 0) {
                const bool had = hasZero;
                hasZero = false;
                return had ? 1 : 0;
            }
            std::size_t hole = slot_of(raw);
            if (hole == npos) return 0;
            // backward shift: pull the following entries of the run into the hole while their home allows it
            const std::size_t mask = table.size() - 1;
            std::size_t next = hole;
            for (;;) {
                next = (next + 1) & mask;
                if (table[next] == 0) break;
                const std::size_t h = home(table[next]);
                const bool movable = next > hole ? (h <= hole or h > next) : (h <= hole and h > next);
                if (not movable) continue;
                table[hole] = table[next];
                hole = next;
            }
            table[hole] = 0;
            --used;
            return 1;
        }

        void clear() {
            table.assign(table.size(), RawId{0});
            used = 0;
            hasZero = false;
        }

        void reserve(size_type wanted) {
            std::size_t slots = 8;
            while (slots < wanted * 2) slots *= 2;
            if (slots > table.size()) rehash(slots);
        }

        bool operator==(const IdSet& other) const {
            if (size() != other.size() or hasZero != other.hasZero) return false;
            for (const RawId raw : table)
                if (raw != 0 and other.slot_of(raw) == npos) return false;
            return true;
        }

    private:
        static constexpr std::size_t npos = static_cast<std::size_t>(-1);

        std::size_t home(RawId raw) const {
            // Fibonacci hashing: ids are random already, but the tests use small values
            const std::uint64_t mixed = static_cast<std::uint64_t>(raw) * 0x9e3779b97f4a7c15ull;
            const int bits = std::countr_zero(table.size());
            return static_cast<std::size_t>(mixed >> (64 - bits));
        }

        std::size_t slot_of(RawId raw) const {
            if (table.empty()) return npos;
            const std::size_t mask = table.size() - 1;
            std::size_t slot = home(raw);
            while (table[slot] != 0) {
                if (table[slot] == raw) return slot;
                slot = (slot + 1) & mask;
            }
            return npos;
        }

        void grow() { rehash(table.empty() ? 8 : table.size() * 2); }

        void rehash(std::size_t slots) {
            std::vector<RawId> old;
            old.swap(table);
            table.assign(slots, RawId{0});
            used = 0;
            for (const RawId raw : old)
                if (raw != 0) insert(Id{raw});
        }

        std::vector<RawId> table;   // size is 0 or a power of two
        std::size_t used = 0;       // used slots, without the zero id
        bool hasZero = false;
    };
}
