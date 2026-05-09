#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace base {

/// Dense vector of entries + unordered map id -> slot index. Mutable; erase uses swap-with-tail.
template<typename IdType, typename ValueType, typename Hasher = std::hash<IdType>, typename KeyEqual = std::equal_to<IdType>>
class DenseTable {
public:
    struct Entry {
        IdType id;
        ValueType value;
    };

    using KeyType = IdType;
    using MappedType = ValueType;
    using SizeType = std::size_t;

    class ConstIterator {
    public:
        struct EntryView {
            const IdType& first;
            const ValueType& second;
        };

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = EntryView;
        using pointer = const EntryView*;
        using reference = const EntryView&;

        EntryView operator*() const {
            return EntryView{entries[index].id, entries[index].value};
        }

        struct ArrowProxy {
            EntryView view;
            const EntryView* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{EntryView{entries[index].id, entries[index].value}};
        }

        ConstIterator& operator++() {
            ++index;
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const ConstIterator& other) const {
            return std::addressof(entries) == std::addressof(other.entries) && index == other.index;
        }

        bool operator!=(const ConstIterator& other) const {
            return !(*this == other);
        }

    private:
        friend class DenseTable;

        ConstIterator(const std::vector<Entry>& entry_vec, SizeType index_)
            : entries(entry_vec)
            , index(index_)
        {}

        const std::vector<Entry>& entries;
        SizeType index;
    };

    DenseTable() = default;

    explicit DenseTable(const Hasher& hash, const KeyEqual& eq = KeyEqual())
        : idToIndex(0, hash, eq)
    {}

    bool contains(const IdType& id) const {
        return idToIndex.find(id) != idToIndex.end();
    }

    const ValueType* find(const IdType& id) const {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    ValueType* find(const IdType& id) {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    const ValueType& at(const IdType& id) const {
        const auto* found = find(id);
        if (!found) throw std::out_of_range("DenseTable::at");
        return *found;
    }

    ValueType& at(const IdType& id) {
        auto* found = find(id);
        if (!found) throw std::out_of_range("DenseTable::at");
        return *found;
    }

    std::optional<ValueType> get(const IdType& id) const {
        const auto* found = find(id);
        if (!found) return std::nullopt;
        return *found;
    }

    SizeType size() const { return entries.size(); }
    bool empty() const { return entries.empty(); }

    void clear() {
        idToIndex.clear();
        entries.clear();
    }

    void reserve(SizeType capacity) {
        idToIndex.reserve(capacity);
        entries.reserve(capacity);
    }

    template<typename IdArg, typename ValueArg>
    void insert(IdArg&& id, ValueArg&& value) {
        const auto lookup = idToIndex.find(id);
        if (lookup != idToIndex.end()) {
            entries[lookup->second].value = std::forward<ValueArg>(value);
            return;
        }
        const SizeType slot = entries.size();
        entries.emplace_back(Entry{std::forward<IdArg>(id), std::forward<ValueArg>(value)});
        idToIndex.emplace(entries.back().id, slot);
    }

    bool erase(const IdType& id) {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return false;

        const SizeType removedSlot = lookup->second;
        idToIndex.erase(lookup);

        if (removedSlot != entries.size() - 1) {
            entries[removedSlot] = std::move(entries.back());
            idToIndex[entries[removedSlot].id] = removedSlot;
        }
        entries.pop_back();
        return true;
    }

    ConstIterator begin() const {
        return ConstIterator(entries, 0);
    }

    ConstIterator end() const {
        return ConstIterator(entries, entries.size());
    }

    std::unordered_map<IdType, SizeType, Hasher, KeyEqual> idToIndex;
    std::vector<Entry> entries;
};

} // namespace base
