#pragma once

#include <cstddef>
#include <optional>

#include <base/cannonball/cursor.h>
#include <base/cannonball/entry.h>

namespace base::cannonball::table {

template<typename Key, typename Val>
class Read {
public:
    using KeyType = Key;
    using MappedType = Val;
    using SizeType = std::size_t;

    using EntryView = base::cannonball::EntryView<Key, Val>;
    using Cursor = base::cannonball::Cursor<Key, Val>;

    virtual ~Read() = default;

    virtual bool contains(const Key& id) const = 0;
    virtual const Val* find(const Key& id) const = 0;
    virtual const Val& at(const Key& id) const = 0;
    virtual std::size_t size() const = 0;

    bool empty() const { return size() == 0; }

    std::optional<Val> get(const Key& id) const {
        if (const auto* found = find(id)) return *found;
        return std::nullopt;
    }

    Cursor begin() const {
        return read_begin();
    }

    Cursor end() const {
        return read_end();
    }

protected:
    virtual Cursor read_begin() const = 0;
    virtual Cursor read_end() const = 0;
};

} // namespace base::cannonball::table
