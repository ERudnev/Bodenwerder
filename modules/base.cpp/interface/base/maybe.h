#pragma once

#include <optional>

namespace base {

    // Thin alias — prefer std::optional semantics (no implicit T& conversion).
    // Presence: has_value() / operator bool. Access: *, ->, value(). Empty: std::nullopt / {}.
    template<typename T>
    using maybe = std::optional<T>;

}
