#pragma once

#include <base/shared_reference.h>

namespace fqsm {
    // Pointer handles used across old iQSM API.
    template<typename T>
    using cref = base::shared_ref<const T>;

    template<typename T>
    using ref = base::shared_ref<T>;

    template<typename T>
    cref<T> freeze(ref<T> r) { return cref<T>(r); }

    template<typename T>
    ref<T> clone(cref<T> r) { return base::make_shared<T>(*r); }
}

