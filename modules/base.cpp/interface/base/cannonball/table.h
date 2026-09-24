#pragma once

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <base/cannonball/entry.h>
#include <base/cannonball/key.h>
#include <base/cannonball/table/direct.h>

namespace base::cannonball {

template<typename Key, typename Val>
class Table : public table::Direct<Key, Val> {
public:
    using Interface = table::Direct<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    using Entry = base::cannonball::Entry<Key, Val>;

    Table() = default;

    bool contains(const Key& id) const override {
        return idToIndex.find(RawKey<Key>::of(id)) != idToIndex.end();
    }

    const Val* find(const Key& id) const override {
        const auto lookup = idToIndex.find(RawKey<Key>::of(id));
        if (lookup == idToIndex.end()) return nullptr;
        return std::addressof(entries[lookup->second].value);
    }

    Val* find(const Key& id) override {
        const auto lookup = idToIndex.find(RawKey<Key>::of(id));
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

    Val& insert(const Key& id, const Val& value) override {
        return insert_impl(id, value);
    }

    Val& insert(Key&& id, Val&& value) override {
        return insert_impl(std::move(id), std::move(value));
    }

    bool erase(const Key& id) override {
        const auto lookup = idToIndex.find(RawKey<Key>::of(id));
        if (lookup == idToIndex.end()) return false;

        const SizeType removedSlot = lookup->second;
        idToIndex.erase(lookup);

        if (removedSlot != entries.size() - 1) {
            entries[removedSlot] = std::move(entries.back());
            idToIndex[RawKey<Key>::of(entries[removedSlot].id)] = removedSlot;
        }

        entries.pop_back();
        return true;
    }

    // Raw backing vector, for composing an overlay Cursor layer on top of this table (see
    // Future::read_begin/read_end); not part of the Read/Direct interface.
    const std::vector<Entry>* raw_entries() const { return &entries; }

protected:
    typename Interface::Cursor read_begin() const override {
        return Interface::Cursor::plain(&entries, 0, this);
    }

    typename Interface::Cursor read_end() const override {
        return Interface::Cursor::plain(&entries, entries.size(), this);
    }

    typename Interface::MutableCursor write_begin() override {
        return typename Interface::MutableCursor(&entries, 0);
    }

    typename Interface::MutableCursor write_end() override {
        return typename Interface::MutableCursor(&entries, entries.size());
    }

private:
    template<typename KeyArg, typename ValArg>
    Val& insert_impl(KeyArg&& id, ValArg&& value) {
        const auto lookup = idToIndex.find(RawKey<Key>::of(id));
        if (lookup != idToIndex.end()) {
            auto& slot = entries[lookup->second].value;
            slot = std::forward<ValArg>(value);
            return slot;
        }

        const SizeType slot = entries.size();
        entries.emplace_back(Entry{std::forward<KeyArg>(id), std::forward<ValArg>(value)});
        idToIndex.emplace(RawKey<Key>::of(entries.back().id), slot);
        return entries.back().value;
    }

    std::unordered_map<typename RawKey<Key>::type, SizeType> idToIndex;
    std::vector<Entry> entries;
};

} // namespace base::cannonball
