#pragma once

#include <base/containers/ImmutableUnorderedMap.h>

#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/meta/global.h>
#include <iQSM/references.h>
#include <iQSM/types.h>

namespace iqsm {
    struct FieldAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldAbstract() = default;
    };

    template<meta::Aspect Meta>
    struct FieldObject final : FieldAbstract {
        using Id = ::iqsm::Id<Meta>;
        using Item = ::iqsm::Item<Meta>;

        using GlobalData = ::iqsm::meta::GlobalData<Meta>;
        using Global = ::iqsm::meta::Global<Meta>;

        using Container = base::ImmutableUnorderedMap<Id, Item>;

        Container container;
        Global global = ::iqsm::meta::zero_global<Meta>();
    };

    template<meta::Aspect Meta>
    using Field = cref<FieldObject<Meta>>;
}

