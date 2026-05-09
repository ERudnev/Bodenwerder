#pragma once

#include <base/containers/denseTable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/alias.h>
#include <iQSM/state/mechanism.h>

namespace iqsm::state::slice {

    struct Abstract {
        virtual ~Abstract() = default;
    };

    template<meta::Aspect Meta, typename Item>
    struct Data : Abstract {
        using Id = ::iqsm::Id<Meta>;
        using Container = base::DenseTable<Id, Item>;

        Container container;
    };
}
    /*
    // abstract (to store within World-like containers)
    struct Abstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~SliceAbstract() = default;
    };

    template<meta::Aspect Meta>
    struct Versioned final : Abstract {
        using Id = ::iqsm::Id<Meta>;
        using Node = ::iqsm::Node<Meta>;
        using Global = ::iqsm::Global<Meta>;
        // due to shared nature of different states of immutable data prenetation, pointers used as data carriers
        using Container = base::containers::DenseTable<Id, Node>;

        // for Versioned, Global data is shared between versions, and zero is one static
        auto zeroGlobal()->::iqsm::Global<Meta>;

        // data:
        Container container;
        Global global = zeroGlobal();
    };


    template<meta::aspect Meta>
    struct Operational : Abstract {
        using Id = ::iqsm::Id<Meta>;
        using Quantum = ::iqsm::Quantum<Meta>;
        using GlobalData = ::iqsm::GlobalData<Meta>;
        using Container = base::containers::DenseTable<Id, Quantum>;

        Container container;
        GlobalData global;
    };
    
}

namespace iqsm::slice::detail {
    template<meta::Aspect Meta>
    auto Versioned<Meta>::zeroGlobal() -> ::iqsm::Global<Meta> {
        static const ::iqsm::Global<Meta> singleton = base::make_shared<const ::iqsm::GlobalData<Meta>>();
        return singleton;
    }
    */
