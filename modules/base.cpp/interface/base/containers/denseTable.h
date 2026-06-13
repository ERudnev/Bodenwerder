#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <base/containers/tableInterface.h>

namespace base {

template<typename IdType, typename ValueType, typename Hasher = std::hash<IdType>, typename KeyEqual = std::equal_to<IdType>>
class DenseTable : public TableInterface<IdType, ValueType> {
public:
    using Interface = TableInterface<IdType, ValueType>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    struct Entry {
        IdType id;
        ValueType value;
    };

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

        ConstIterator(const std::vector<Entry>& entries, SizeType index)
            : entries(entries)
            , index(index)
        {}

        const std::vector<Entry>& entries;
        SizeType index;
    };

    class Iterator {
    public:
        struct EntryRef {
            const IdType& first;
            ValueType& second;
        };

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = EntryRef;
        using pointer = EntryRef*;
        using reference = EntryRef&;

        EntryRef operator*() const {
            return EntryRef{entries[index].id, entries[index].value};
        }

        struct ArrowProxy {
            EntryRef view;
            const EntryRef* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{EntryRef{entries[index].id, entries[index].value}};
        }

        Iterator& operator++() {
            ++index;
            return *this;
        }

        Iterator operator++(int) {
            Iterator copy = *this;
            ++*this;
            return copy;
        }

        bool operator==(const Iterator& other) const {
            return std::addressof(entries) == std::addressof(other.entries) && index == other.index;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        friend class DenseTable;

        Iterator(std::vector<Entry>& entries, SizeType index)
            : entries(entries)
            , index(index)
        {}

        std::vector<Entry>& entries;
        SizeType index;
    };

    DenseTable() = default;

    explicit DenseTable(const Hasher& hash, const KeyEqual& equal = KeyEqual())
        : idToIndex(0, hash, equal)
    {}

    bool contains(const IdType& id) const override {
        return idToIndex.find(id) != idToIndex.end();
    }

    const ValueType* find(const IdType& id) const override {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    ValueType* find(const IdType& id) override {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    const ValueType& at(const IdType& id) const override {
        const auto* found = find(id);
        if (!found) throw std::out_of_range("DenseTable::at");
        return *found;
    }

    ValueType& at(const IdType& id) override {
        auto* found = find(id);
        if (!found) throw std::out_of_range("DenseTable::at");
        return *found;
    }

    SizeType size() const override {
        return entries.size();
    }

    void clear() override {
        idToIndex.clear();
        entries.clear();
    }

    void reserve(SizeType capacity) override {
        idToIndex.reserve(capacity);
        entries.reserve(capacity);
    }

    void insert(const IdType& id, const ValueType& value) override {
        insert_impl(id, value);
    }

    void insert(IdType&& id, ValueType&& value) override {
        insert_impl(std::move(id), std::move(value));
    }

    bool erase(const IdType& id) override {
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

    std::unordered_map<IdType, SizeType, Hasher, KeyEqual> idToIndex;
    std::vector<Entry> entries;

protected:
    typename Interface::ReadIterator read_begin() const override {
        return this->make_read_iterator(ConstIterator(entries, 0));
    }

    typename Interface::ReadIterator read_end() const override {
        return this->make_read_iterator(ConstIterator(entries, entries.size()));
    }

    typename Interface::WriteIterator write_begin() override {
        return this->make_write_iterator(Iterator(entries, 0));
    }

    typename Interface::WriteIterator write_end() override {
        return this->make_write_iterator(Iterator(entries, entries.size()));
    }

private:
    template<typename IdArg, typename ValueArg>
    void insert_impl(IdArg&& id, ValueArg&& value) {
        const auto lookup = idToIndex.find(id);
        if (lookup != idToIndex.end()) {
            entries[lookup->second].value = std::forward<ValueArg>(value);
            return;
        }

        const SizeType slot = entries.size();
        entries.emplace_back(Entry{std::forward<IdArg>(id), std::forward<ValueArg>(value)});
        idToIndex.emplace(entries.back().id, slot);
    }
};

} // namespace base
