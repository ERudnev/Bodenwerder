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
class DenseTable : public table::Direct<Key, Val> {
public:
    using Interface = table::Direct<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    struct Entry {
        Key key;
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
            return value_type{entries[index].key, entries[index].value};
        }

        struct ArrowProxy {
            value_type view;
            const value_type* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{value_type{entries[index].key, entries[index].value}};
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
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = typename Interface::EntryRef;
        using pointer = value_type*;
        using reference = value_type&;

        value_type operator*() const {
            return value_type{entries[index].key, entries[index].value};
        }

        struct ArrowProxy {
            value_type view;
            const value_type* operator->() const { return &view; }
        };

        ArrowProxy operator->() const {
            return ArrowProxy{value_type{entries[index].key, entries[index].value}};
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
        : keyToIndex(0, hash, equal)
    {}

    bool contains(const Key& key) const override {
        return keyToIndex.find(key) != keyToIndex.end();
    }

    const Val* find(const Key& key) const override {
        const auto lookup = keyToIndex.find(key);
        if (lookup == keyToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    Val* find(const Key& key) override {
        const auto lookup = keyToIndex.find(key);
        if (lookup == keyToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    const Val& at(const Key& key) const override {
        const auto* found = find(key);
        if (!found) throw std::out_of_range("DenseTable::at");
        return *found;
    }

    Val& at(const Key& key) override {
        auto* found = find(key);
        if (!found) throw std::out_of_range("DenseTable::at");
        return *found;
    }

    SizeType size() const override {
        return entries.size();
    }

    void clear() override {
        keyToIndex.clear();
        entries.clear();
    }

    void reserve(SizeType capacity) override {
        keyToIndex.reserve(capacity);
        entries.reserve(capacity);
    }

    void insert(const Key& key, const Val& value) override {
        insert_impl(key, value);
    }

    void insert(Key&& key, Val&& value) override {
        insert_impl(std::move(key), std::move(value));
    }

    bool erase(const Key& key) override {
        const auto lookup = keyToIndex.find(key);
        if (lookup == keyToIndex.end()) return false;

        const SizeType removedSlot = lookup->second;
        keyToIndex.erase(lookup);

        if (removedSlot != entries.size() - 1) {
            entries[removedSlot] = std::move(entries.back());
            keyToIndex[entries[removedSlot].key] = removedSlot;
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
    void insert_impl(KeyArg&& key, ValArg&& value) {
        const auto lookup = keyToIndex.find(key);
        if (lookup != keyToIndex.end()) {
            entries[lookup->second].value = std::forward<ValArg>(value);
            return;
        }

        const SizeType slot = entries.size();
        entries.emplace_back(Entry{std::forward<KeyArg>(key), std::forward<ValArg>(value)});
        keyToIndex.emplace(entries.back().key, slot);
    }

    std::unordered_map<Key, SizeType, Hasher, KeyEqual> keyToIndex;
    std::vector<Entry> entries;
};

} // namespace base::cannonball