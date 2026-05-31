#pragma once

#include <fQSM/state/details/aware_at.h>
#include <fQSM/state/composite.h>
#include <fQSM/schema/dag.h>

namespace fqsm::state::world {
    namespace axis = meta::axis;

    struct View {
        using AbstractSlice = slice::Abstract<axis::order::state>;

        template<aspect::Any Meta>
        using TableView = cref<slice::View<Meta, axis::order::state>>;

        template<aspect::Any Meta>
        using ItemsView = typename slice::View<Meta, axis::order::state>::ItemsView;

        template<aspect::Any Meta>
        auto slice() const -> TableView<Meta> {
            return base::shared_ref_cast<const slice::View<Meta, axis::order::state>>(
                slice(aspect::Rtid::of<Meta>())
            );
        }

        template<aspect::Any Meta>
        auto items() const -> const ItemsView<Meta>& { return slice<Meta>()->items(); }

        const Schema schema;

    protected:
        explicit View(Schema schema) : schema(schema) {}

        virtual auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> = 0;
    };

    struct Data : View {
        using CompositeData = composite::Data<axis::order::state>;

        template<aspect::Any Meta>
        using TableData = typename CompositeData::Entry::template Handle<Meta>;

        template<aspect::Any Meta>
        using ItemsData = typename slice::Data<Meta, axis::order::state>::ItemsData;

        using View::items;
        using View::slice;

        explicit Data(Schema schema) : View(schema) {
            for (const auto& [aspectId, aspect] : schema->nodes) {
                composite().slices.emplace(aspectId, aspect.binding.createState());
            }
        }

        explicit Data(const View& source) : View(source.schema) {
            for (const auto& [aspectId, aspect] : schema->nodes) {
                composite().slices.emplace(aspectId, aspect.binding.cloneState(source));
            }
        }

        auto composite() -> CompositeData& { return slices; }

        template<aspect::Any Meta>
        auto slice() -> TableData<Meta> { return composite().template slice<Meta>(); }

        template<aspect::Any Meta>
        auto items() -> ItemsData<Meta>& { return slice<Meta>()->items(); }

    protected:
        auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> override { return aware_at(slices.slices, runtimeTypeId); }

    private:
        CompositeData slices;
    };
}