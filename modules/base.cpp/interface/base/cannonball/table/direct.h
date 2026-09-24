#pragma once

#include <cstddef>
#include <utility>

#include <base/cannonball/cursor.h>
#include <base/cannonball/entry.h>
#include <base/cannonball/table/operational.h>

namespace base::cannonball::table {

template<typename Key, typename Val>
class Direct : public Operational<Key, Val> {
public:
    using Interface = Operational<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    using EntryView = typename Interface::EntryView;
    using EntryRef = base::cannonball::EntryRef<Key, Val>;
    using MutableCursor = base::cannonball::MutableCursor<Key, Val>;

    virtual ~Direct() = default;

    using Interface::at;
    using Interface::begin;
    using Interface::end;
    using Interface::find;

    virtual Val* find(const Key& id) = 0;
    virtual Val& at(const Key& id) = 0;

    MutableCursor begin() {
        return write_begin();
    }

    MutableCursor end() {
        return write_end();
    }

protected:
    virtual MutableCursor write_begin() = 0;
    virtual MutableCursor write_end() = 0;
};

} // namespace base::cannonball::table
