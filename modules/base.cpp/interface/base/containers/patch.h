#pragma once

#include <optional>
#include <base/containers/table.h>

namespace base::patch {
    template<typename Val>
    using Element = std::optional<Val>;
}

namespace base {
    template<typename Key, typename Val>
    using Patch = Table<Key, Val>;
}
