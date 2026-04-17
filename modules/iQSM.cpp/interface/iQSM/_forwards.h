#pragma once

#include <iQSM/references.h>

namespace iqsm {

    // core entities
    struct SchemaObject;
    using Schema = cref<SchemaObject>;

    struct WorldObject;
    using World = cref<WorldObject>;

    namespace delta { struct Fields; }
    using Delta = cref<delta::Fields>;

    namespace resources { 
        struct ManagerCore;
        using Provider = cref<ManagerCore>;
    }

}


