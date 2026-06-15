#pragma once

#include <base/cannonball/delta/interface.h>
#include <base/cannonball/table/read.h>

namespace base::cannonball::delta {

template<typename Key, typename Val>
class DoubleBuffered : public Interface<Key, Val> {
public:
    using View = table::Read<Key, Val>;

    DoubleBuffered(const View& before, const View& after)
        : before(before)
        , after(after)
    {}

private:
    const View& before;
    const View& after;
};

} // namespace base::cannonball::delta