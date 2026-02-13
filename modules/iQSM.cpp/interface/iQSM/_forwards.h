#pragma once

#include <memory>

namespace iqsm {
    // pointer handles:
    template<typename T>
    using cref = std::shared_ptr<const T>;

    template<typename T>
    using ref = std::shared_ptr<T>;

    template<typename T>
    cref<T> freeze(ref<T> r) { return r; }

    template<typename T>
    ref<T> clone(cref<T> r) { return r ? std::make_shared<T>(*r) : nullptr; }

    // core handles:
    struct SchemaObject;
    using Schema = cref<SchemaObject>;

    struct WorldObject;
    using World = cref<WorldObject>;

    namespace delta { struct WorldState; }
    using Delta = cref<delta::WorldState>;
}

namespace iqsm::ops::validation {
    using Func = Delta(*)(World);
}


