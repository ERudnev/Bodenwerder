#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <base/cannonball/table/direct.h>

namespace base::cannonball {

template<typename Key, typename Val, typename Hasher = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class Table : public table::Direct<Key, Val> {
public:
    using Interface = table::Direct<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    struct Entry {
        Key id;
        Val value;
    };

    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = typename Interface::EntryView;
        using pointer = const value_type*;
        using reference = const value_type&;

        value_type operator*() const {
            return value_type{entries[index].id, entries[index].value};
        }

        struct ArrowProxy {
            value_type view;
            const value_type* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{value_type{entries[index].id, entries[index].value}};
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
        friend class Table;

        ConstIterator(const std::vector<Entry>& entries, SizeType index)
            : entries(entries)
            , index(index)
        {}

        const std::vector<Entry>& entries;
        SizeType index;
    };

    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = typename Interface::EntryRef;
        using pointer = value_type*;
        using reference = value_type&;

        value_type operator*() const {
            return value_type{entries[index].id, entries[index].value};
        }

        struct ArrowProxy {
            value_type view;
            const value_type* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{value_type{entries[index].id, entries[index].value}};
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
        friend class Table;

        Iterator(std::vector<Entry>& entries, SizeType index)
            : entries(entries)
            , index(index)
        {}

        std::vector<Entry>& entries;
        SizeType index;
    };

    Table() = default;

    explicit Table(const Hasher& hash, const KeyEqual& equal = KeyEqual())
        : idToIndex(0, hash, equal)
    {}

    bool contains(const Key& id) const override {
        return idToIndex.find(id) != idToIndex.end();
    }

    const Val* find(const Key& id) const override {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    Val* find(const Key& id) override {
        const auto lookup = idToIndex.find(id);
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    const Val& at(const Key& id) const override {
        const auto* found = find(id);
        if (!found) throw std::out_of_range("Table::at");
        return *found;
    }

    Val& at(const Key& id) override {
        auto* found = find(id);
        if (!found) throw std::out_of_range("Table::at");
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

    void insert(const Key& id, const Val& value) override {
        insert_impl(id, value);
    }

    void insert(Key&& id, Val&& value) override {
        insert_impl(std::move(id), std::move(value));
    }

    bool erase(const Key& id) override {
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
    template<typename KeyArg, typename ValArg>
    void insert_impl(KeyArg&& id, ValArg&& value) {
        const auto lookup = idToIndex.find(id);
        if (lookup != idToIndex.end()) {
            entries[lookup->second].value = std::forward<ValArg>(value);
            return;
        }

        const SizeType slot = entries.size();
        entries.emplace_back(Entry{std::forward<KeyArg>(id), std::forward<ValArg>(value)});
        idToIndex.emplace(entries.back().id, slot);
    }

    std::unordered_map<Key, SizeType, Hasher, KeyEqual> idToIndex;
    std::vector<Entry> entries;
};

} // namespace base::cannonball