#pragma once

#include <utility>

#include <iQSM/meta/aspect.h>
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

        template<aspect::Any Meta>
        auto slice() const -> cref<typename Meta::Runtime::ValueSlice>;

        template<aspect::Any Meta>
        auto experimental_template_slice_access() const -> cref<

    protected:
        explicit View(Schema schema) : id(Id::generate_random()), schema(std::move(schema)) {}

        virtual
        auto slice(RAId runtimeTypeId) const -> cref<slice::AbstractState<axis::versioning> = 0;
    };
}

namespace iqsm::state {
    template<aspect::Any Meta>
    auto View::slice() const -> cref<typename Meta::Runtime::ValueSlice> {
        using Slice = typename Meta::Runtime::ValueSlice;
        return base::shared_ref_cast<const Slice>(slice(RAId{typeid(Slice)}));
    }
}
