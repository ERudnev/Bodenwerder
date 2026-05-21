#pragma once

#include <base/shared_reference.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/state/composite.h>
#include <fQSM/state/schema.h>

namespace fqsm::state::world {
    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

    struct Patch {
        using CompositeData = composite::Data<axis::order::patch>;

        template<aspect::Any Meta>
        using TableData = typename CompositeData::Entry::template Handle<Meta>;

        template<aspect::Any Meta>
        using ItemsData = typename slice::Data<Meta, axis::order::patch>::ItemsData;

        explicit Patch(Schema schema) : schema(schema) {}

        auto composite() -> CompositeData& { return slices; }

        template<aspect::Any Meta>
        auto slice() -> TableData<Meta> {
            const auto aspectId = aspect::Rtid::of<Meta>();
            if (!slices.slices.contains(aspectId)) {
                slices.slices.emplace(aspectId, base::make_shared<slice::Data<Meta, axis::order::patch>>());
            }
            return composite().template slice<Meta>();
        }

        template<aspect::Any Meta>
        auto items() -> ItemsData<Meta>& { return slice<Meta>()->items(); }

        const Schema schema;

    private:
        CompositeData slices;
    };
}