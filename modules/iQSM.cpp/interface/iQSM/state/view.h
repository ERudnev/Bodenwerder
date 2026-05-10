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

        template<meta::Aspect Meta>
        auto slice() const -> cref<typename Meta::Runtime::ValueSlice>;

    protected:
        explicit View(Schema schema) : id(Id::generate_random()), schema(std::move(schema)) {}

        virtual
        auto slice(RAId runtimeTypeId) const -> cref<slice::Abstract> = 0;
    };
}

namespace iqsm::state {
    template<meta::Aspect Meta>
    auto View::slice() const -> cref<typename Meta::Runtime::ValueSlice> {
        using Slice = typename Meta::Runtime::ValueSlice;
        return base::shared_ref_cast<const Slice>(slice(RAId{typeid(Slice)}));
    }
}
