#pragma once

#include <fQSM/meta/concepts.h>
#include <fQSM/state/composite.h>
#include <fQSM/state/schema.h>

namespace fqsm::state::world {
    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

    struct View {
        using CompositeView = composite::View<axis::order::state>;

        template<aspect::Any Meta>
        using TableView = typename CompositeView::Entry::template Handle<Meta>;

        template<aspect::Any Meta>
        using ItemsView = typename slice::View<Meta, axis::order::state>::ItemsView;

        template<aspect::Any Meta>
        auto slice() const -> TableView<Meta> { return composite().template slice<Meta>(); }

        template<aspect::Any Meta>
        auto items() const -> const ItemsView<Meta>& { return slice<Meta>()->items(); }

        const Schema schema;

    protected:
        explicit View(Schema schema) : schema(schema) {}

        virtual auto composite() const -> const CompositeView& = 0;
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
            for (const auto& [aspectId, aspect] : schema->aspects) {
                composite().slices.emplace(aspectId, aspect.factory.createState());
            }
        }

        explicit Data(const View& source) : View(source.schema) {
            for (const auto& [aspectId, aspect] : schema->aspects) {
                composite().slices.emplace(aspectId, aspect.factory.cloneState(source));
            }
        }

        auto composite() -> CompositeData& { return slices; }

        template<aspect::Any Meta>
        auto slice() -> TableData<Meta> { return composite().template slice<Meta>(); }

        template<aspect::Any Meta>
        auto items() -> ItemsData<Meta>& { return slice<Meta>()->items(); }

    protected:
        auto composite() const -> const CompositeView& override { return slices; }

    private:
        CompositeData slices;
    };
}