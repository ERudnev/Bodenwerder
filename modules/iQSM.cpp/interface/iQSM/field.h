#pragma once

#include <base/containers/ImmutableUnorderedMap.h>
//#include <base/containers/denseTable.h>

#include <iQSM/collections/field.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/meta/global.h>
#include <iQSM/references.h>

namespace iqsm {

    template<meta::Aspect Meta>
    struct FieldData final : FieldAbstract {
        using Id = ::iqsm::Id<Meta>;
        using Item = ::iqsm::Item<Meta>;

        using GlobalData = ::iqsm::meta::GlobalData<Meta>;
        using Global = ::iqsm::meta::Global<Meta>;

        using Container = base::ImmutableUnorderedMap<Id, Item>;
        //using Container = base::DenseTable<Id,Item>;

        Container container;
        Global global = ::iqsm::meta::zero_global<Meta>();
    };

    template<meta::Aspect Meta>
    using Field = cref<FieldData<Meta>>;
}

