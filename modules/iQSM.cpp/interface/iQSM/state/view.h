#pragma once

#include <utility>

#include <iQSM/typeId.h>
#include <iQSM/identifier.h>
#include <iQSM/state/slice.h>
#include <iQSM/state/_forwards.h>

namespace iqsm::state {

    struct View {
        using Id = Identifier<View>;

        const Id id;
        const Schema schema;

        virtual World share() const = 0;

    protected:
        explicit View(Schema schema) : id(Id::generate_random()), schema(std::move(schema)) {}

        virtual
        auto slice(RAId runtimeTypeId) const -> cref<slice::Abstract> = 0;

        /* finish it some day
        template<meta::Aspect Meta, template<meta::Aspect> typename Cell>
        auto slice() const -> cref<slice::Data<Meta, Cell>>;
        */
    };
}

namespace iqsm::state {
    /*
    template<meta::Aspect Meta, template<meta::Aspect> typename Cell>
    auto View::slice() const -> cref<slice::Data<Meta, Cell>> {
        using Slice = slice::Data<Meta, Cell>;
        return base::shared_ref_cast<const Slice>(slice(RAId{typeid(Slice)}));
    }
    */
}
