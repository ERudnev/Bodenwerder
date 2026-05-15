#pragma once

#include <unordered_map>
#include <optional>

#include <base/maybe.h>
#include <iQSM/typeId.h>
#include <iQSM/meta/axis.h>
#include <iQSM/meta/alias.h>
#include <iQSM/meta/concepts/archetype.h>
#include <iQSM/state/_forwards.h>


namespace iqsm::meta::state {
    template<archetype::Any Meta, axis::versioning, axis::order>
    struct ItemsLayout;
}


/* TODO: cleanp if Registration:: absorb this alias
namespace iqsm::state {
    template<meta::Aspect Meta, axis::versioning ItemsVersioning, axis::order Order>
    using Chunk = typename detail::ItemsLayout<Meta, ItemsVersioning, Order>::Element;

    template<axis::versioning SliceVersioning>
    using SlicesLayout = detail::SlicesLayout<SliceVersioning>;
    
    //template<axis::versioning OperationalVersioning>
}
*/

namespace iqsm::meta::state {

    namespace patch {
        // TODO: find better alias as template<versioning>...

        //template<typename Meta>
        //using FlatPatch = std::optional<Quantum<Meta>>;
        template<archetype::Any Meta>
        struct Solid : base::maybe<Quantum<Meta>> {
            bool is_noop() const { return exists(); }
        };

        template<archetype::Any Meta>
        struct Shared {
            std::optional<Node<Meta>> before;
            std::optional<Node<Meta>> after;

            bool is_noop() const { return !before && !after; }
            bool is_add()  const { return !before &&  after; }
            bool is_del()  const { return  before && !after; }
            bool is_chg()  const { return  before &&  after; }
        };
    }

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::shared, axis::order::state> {
        using Element = Node<Meta>;
    };

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::shared, axis::order::patch> {
        using Element = patch::Shared<Meta>;
    };

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::single, axis::order::state> {
        using Element = Quantum<Meta>;
    };

    template<archetype::Any Meta>
    struct ItemsLayout<Meta, axis::versioning::single, axis::order::patch> {
        using Element = patch::Solid<Meta>;
    };

    /*
    template<>
    struct SlicesLayout<axis::versioning::shared> {  
        template<typename T>
        using RefQualified = iqsm::cref<T>;
        using SlicesContainer = std::unordered_map<RAId, RefQualified<slice::Abstract>>; // TODO: std::map -> base::DenseTable
    };

    template<>
    struct SlicesLayout<axis::versioning::single> {
        template<typename T>
        using RefQualified = iqsm::ref<T>;
        using SlicesContainer = std::unordered_map<RAId, RefQualified<slice::Abstract>>; // TODO: std::map -> base::DenseTable
    };
    */
}