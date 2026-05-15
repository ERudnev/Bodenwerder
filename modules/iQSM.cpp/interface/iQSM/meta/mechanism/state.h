#pragma once

#include <unordered_map>
#include <optional>

#include <iQSM/typeId.h>
#include <iQSM/meta/alias.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/patches.h>


namespace iqsm::meta::state {
    template<meta::Aspect Meta, policy::versioning, policy::order>
    struct ItemsLayout;
}


/* TODO: cleanp if Registration:: absorb this alias
namespace iqsm::state {
    template<meta::Aspect Meta, policy::versioning ItemsVersioning, policy::order Order>
    using Chunk = typename detail::ItemsLayout<Meta, ItemsVersioning, Order>::Element;

    template<policy::versioning SliceVersioning>
    using SlicesLayout = detail::SlicesLayout<SliceVersioning>;
    
    //template<policy::versioning OperationalVersioning>
}
*/

namespace iqsm::meta::state {

    namespace patch {
        // TODO: find better alias as template<versioning>...

        //template<typename Meta>
        //using FlatPatch = std::optional<Quantum<Meta>>;
        template<aspect::Any Meta>
        struct Solid : base::maybe<Quantum<Meta>> {
            bool is_noop() const { return exists(); }
        };

        template<aspect::Any Meta>
        struct Shared {
            std::optional<Node<Meta>> before;
            std::optional<Node<Meta>> after;

            bool is_noop() const { return !before && !after; }
            bool is_add()  const { return !before &&  after; }
            bool is_del()  const { return  before && !after; }
            bool is_chg()  const { return  before &&  after; }
        };
    }

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::shared, policy::order::state> {
        using Element = Node<Meta>;
    };

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::shared, policy::order::patch> {
        using Element = VersionedPatch<Meta>;
    };

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::single, policy::order::state> {
        using Element = Quantum<Meta>;
    };

    template<meta::Aspect Meta>
    struct ItemsLayout<Meta, policy::versioning::single, policy::order::patch> {
        using Element = FlatPatch<Meta>;
    };

    /*
    template<>
    struct SlicesLayout<policy::versioning::shared> {  
        template<typename T>
        using RefQualified = iqsm::cref<T>;
        using SlicesContainer = std::unordered_map<RAId, RefQualified<slice::Abstract>>; // TODO: std::map -> base::DenseTable
    };

    template<>
    struct SlicesLayout<policy::versioning::single> {
        template<typename T>
        using RefQualified = iqsm::ref<T>;
        using SlicesContainer = std::unordered_map<RAId, RefQualified<slice::Abstract>>; // TODO: std::map -> base::DenseTable
    };
    */
}