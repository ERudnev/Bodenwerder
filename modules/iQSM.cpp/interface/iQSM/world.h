#pragma once

#include <memory>
#include <optional>
#include <typeindex>

#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/basis.h>
#include <iQSM/field.h>
#include <iQSM/aspects.h>

namespace iqsm {

    struct WorldValue {
        using Container = base::ImmutableUnorderedMap<FieldUntyped::RuntimeTypeId, UField>;

        Basis basis;
        Container fields;

        template<Facet Meta>
        Field<Meta> Field() const {
            // do not implement!
        }

    };

    // Handles
    using World = cref<WorldValue>;


}
