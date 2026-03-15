#pragma once

#include <memory>
#include <utility>

#include <iQSM/schema.h>

namespace iqsm::ops::schema {
    template<meta::Aspect... Leaves>
    inline Schema assemble() {
        return base::make_shared<const SchemaObject>(SchemaObject::assemble<Leaves...>());
    }

    inline Schema create(SchemaObject schema) {
        return base::make_shared<const SchemaObject>(std::move(schema));
    }
}

