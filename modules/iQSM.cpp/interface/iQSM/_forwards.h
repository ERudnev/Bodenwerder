#pragma once

#include <base/shared_reference.h>

namespace iqsm {
    // pointer handles:
    template<typename T>
    using cref = base::shared_ref<const T>;

    template<typename T>
    using ref = base::shared_ref<T>;

    template<typename T>
    cref<T> freeze(ref<T> r) { return cref<T>(std::move(r)); }

    template<typename T>
    ref<T> clone(cref<T> r) { return base::make_shared<T>(*r); }

    // core handles:
    struct SchemaObject;
    using Schema = cref<SchemaObject>;

    struct WorldObject;
    using World = cref<WorldObject>;

    namespace delta { struct Fields; }
    using Delta = cref<delta::Fields>;

    namespace internals { struct FieldsMutable; }
    namespace repo { struct Commit; }
}

namespace iqsm::ops::validation {
    using Func = void(*)(iqsm::repo::Commit);
}


