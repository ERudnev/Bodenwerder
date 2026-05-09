#pragma once

#include <iQSM/typeId.h>
#include <iQSM/identifier.h>
#include <iQSM/state/slice.h>

// forwards:
#include <iQSM/state/interface.h>

namespace iqsm::state {

    struct View {
        using Id = Identifier<View>;
        using SliceId = internals::Types::RuntimeId;

        const Id id;
        const Schema schema;

        virtual
        auto slice(SliceId runtimeTypeId) const -> cref<slice::Abstract> = 0;


        template<meta::Aspect Meta, template<meta::Aspect> typename Cell>
        auto slice() const -> cref<slice::Data<Meta, Cell>>;
    };
}

namespace iqsm::state {
    template<meta::Aspect Meta, template<meta::Aspect> typename Cell>
    auto View::slice() const -> cref<slice::Data<Meta, Cell>> {
        using Slice = slice::Data<Meta, Cell>;
        return base::shared_ref_cast<const Slice>(slice(SliceId{typeid(Slice)}));
    }
}
