#pragma once

#include <cstddef>
#include <utility>

#include <base/cannonball/table/read.h>

namespace base::cannonball::table {

template<typename Key, typename Val>
class Operational : public Read<Key, Val> {
public:
    using Interface = Read<Key, Val>;
    using KeyType = typename Interface::KeyType;
    using MappedType = typename Interface::MappedType;
    using SizeType = typename Interface::SizeType;

    virtual ~Operational() = default;

    virtual void clear() = 0;
    virtual void reserve(SizeType capacity) = 0;
    virtual void insert(const Key& id, const Val& value) = 0;
    virtual void insert(Key&& id, Val&& value) = 0;
    virtual bool erase(const Key& id) = 0;
};

} // namespace base::cannonball::table
