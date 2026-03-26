#pragma once

#include <memory>
#include <utility>

#include <iQSM/internals/schema_assemble.h>

namespace iqsm::helpers::schema {
    template<meta::Aspect... Leaves>
    inline Schema assemble() {
        return base::make_shared<const SchemaObject>(SchemaObject::assemble<Leaves...>());
    }

    inline Schema create(SchemaObject schema) {
        return base::make_shared<const SchemaObject>(std::move(schema));
    }
}

