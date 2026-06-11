#pragma once

#include <base/containers/denseTable.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/specializations.h>

// local usings and forwards
namespace fqsm::state::slice {
    namespace axis = meta::axis;

    template<aspect::Any Meta>
    struct Delta;
}

namespace fqsm::state::slice {    

    template<axis::order Order>
    struct Abstract {
        virtual ~Abstract() = default;
        virtual bool tainted() const { return false; }
    };

    template<aspect::Any Meta, axis::order Order>
    struct View : Abstract<Order> {
        using Item = typename meta::state::DataLayout<Meta, Order>::Item;
        using Global = typename meta::state::DataLayout<Meta, Order>::Global;
        using ItemsView = base::TableView<Id<Meta>, Item>;

        virtual const ItemsView& items() const = 0;
        virtual const Global& global() const = 0;
    };

    
}
