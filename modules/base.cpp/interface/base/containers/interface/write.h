#pragma once

#include <cstddef>
#include <utility>

#include <base/containers/interface/read.h>

namespace base::table {

template<typename Key, typename Val>
class Write : public Read<Key, Val> {
public:
    using Interface = Read<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = std::size_t;

    using EntryView = typename Interface::EntryView;
    using ReadIterator = typename Interface::ReadIterator;

    virtual ~Write() = default;

    virtual void clear() = 0;
    virtual void reserve(SizeType capacity) = 0;
    virtual void insert(const Key& key, const Val& value) = 0;
    virtual void insert(Key&& key, Val&& value) = 0;
    virtual bool erase(const Key& key) = 0;
};

} // namespace base::table
