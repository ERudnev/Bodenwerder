#pragma once

#include <base/shared_reference.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/state/composite.h>
#include <fQSM/schema/dag.h>

namespace fqsm::state::world {
    namespace axis = meta::axis;

    struct Patch {
        using CompositeView = composite::View<axis::order::patch>;
        using CompositeData = composite::Data<axis::order::patch>;

        template<aspect::Any Meta>
        using TableView = typename CompositeView::Entry::template Handle<Meta>;

        template<aspect::Any Meta>
        using TableData = typename CompositeData::Entry::template Handle<Meta>;

        template<aspect::Any Meta>
        using ItemsData = typename slice::Data<Meta, axis::order::patch>::ItemsData;

        explicit Patch(Schema schema) : schema(schema) {}

        auto composite() -> CompositeData& { return slices; }

        template<aspect::Any Meta>
        auto slice() const -> TableView<Meta> {
            const auto aspectId = aspect::Rtid::of<Meta>();
            const auto found = slices.slices.find(aspectId);
            if (found == slices.slices.end()) return empty_slice<Meta>();
            return base::shared_ref_cast<const typename CompositeView::Entry::template Typed<Meta>>(found->second);
        }

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
        template<aspect::Any Meta>
        static auto empty_slice() -> TableView<Meta> {
            static const auto empty = fqsm::freeze(base::make_shared<slice::Data<Meta, axis::order::patch>>());
            return empty;
        }

        CompositeData slices;
    };
}