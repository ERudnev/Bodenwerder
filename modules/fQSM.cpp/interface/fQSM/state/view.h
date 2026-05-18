#pragma once

#include <utility>

#include <fQSM/meta/concepts.h>
#include <fQSM/typeId.h>
#include <fQSM/identifier.h>
#include <fQSM/state/slice.h>
#include <fQSM/state/composite.h>
#include <fQSM/state/_forwards.h>

namespace fqsm::state {

    struct View {
        // alias:
        struct Reading { //local meanings of View==Reading is duplicated to allow descendant types...
            using Structure = Composite<axis::order::state, axis::mutability::constant>;
        
            template<aspect::Any Meta>
            using Slice = typename Structure::Entry::template TypedHandle<Meta>;
        };

        // access:
        template<aspect::Any Meta>
        auto slice() const -> Reading::Slice<Meta>;

        // value:
        const Schema schema;       

    protected:
        explicit View(Schema schema) : schema(schema) {}

        virtual
        auto slice(RAId runtimeTypeId) const -> cref<slice::Abstract<meta::axis::order::state>> = 0;
    };
}

namespace fqsm::state {
    template<aspect::Any Meta>
    auto View::slice() const -> Reading::Slice<Meta> {
        using Slice = typename Reading::Structure::Entry::template Typed<Meta>;
        return base::shared_ref_cast<const Slice>(slice(RAId{typeid(Slice)}));
    }
}
