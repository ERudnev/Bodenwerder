#pragma once

#include <memory>
#include <utility>

#include <iQSM/schema.h>

namespace iqsm::ops::schema {
    template<Facet... Leaves>
    inline Schema assemble() {
        return std::make_shared<const SchemaObject>(SchemaObject::assemble<Leaves...>());
    }

    inline Schema create(SchemaObject schema) {
        return std::make_shared<const SchemaObject>(std::move(schema));
    }
}

