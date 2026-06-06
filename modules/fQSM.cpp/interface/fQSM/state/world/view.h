#pragma once

#include <fQSM/schema/dag.h>
#include <fQSM/state/slice/view.h>

// Some terms and rationale:
// 1. State is more than just a full bucket of objects; State must also keep Connections.
// 2. View must not know about this complex stuff; View is for Aspect::Quantum items.
// 2.1 There is a strong physical analogy here: gluons exist, but they are not directly observable.
//     Connections belong to State, but are not accessible through View.
// 3. "Matter World" is the world of real object data (Aspect::Quantum).
// 4. "Connections" are never part of business logic. They are only an optimization cache.

namespace fqsm::state::world {
    namespace axis = meta::axis;

    struct View {
        using AbstractSlice = slice::Abstract<axis::order::state>;

        template<aspect::Any Meta>
        using TableView = cref<slice::View<Meta, axis::order::state>>;

        template<aspect::Any Meta>
        using ItemsView = typename slice::View<Meta, axis::order::state>::ItemsView;

        template<aspect::Any Meta>
        using GlobalView = typename slice::View<Meta, axis::order::state>::Global;

        template<aspect::Any Meta>
        auto slice() const -> TableView<Meta> {
            return base::shared_ref_cast<const slice::View<Meta, axis::order::state>>(
                slice(aspect::Rtid::of<Meta>())
            );
        }

        template<aspect::Any Meta>
        auto items() const -> const ItemsView<Meta>& { return slice<Meta>()->items(); }

        template<aspect::Any Meta>
        auto global() const -> const GlobalView<Meta>& { return slice<Meta>()->global(); }

        const Schema schema;

    protected:
        explicit View(Schema schema) : schema(schema) {}

        virtual auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> = 0;
    };

}