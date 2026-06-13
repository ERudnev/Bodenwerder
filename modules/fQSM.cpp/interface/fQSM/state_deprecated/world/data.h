#pragma once

#include <fQSM/state/composite.h>
#include <fQSM/state/connections/world.h>
#include <fQSM/state/details/aware_at.h>
#include <fQSM/state/world/view.h>

namespace fqsm::state::world {
    namespace axis = meta::axis;

    struct Data : View {
        using MaterialData = composite::Data<axis::order::state>;

        template<aspect::Any Meta>
        using TableData = typename MaterialData::Entry::template Handle<Meta>;

        template<aspect::Any Meta>
        using ItemsData = typename slice::Data<Meta, axis::order::state>::ItemsData;

        template<aspect::Any Meta>
        using GlobalData = typename slice::Data<Meta, axis::order::state>::Global;

        //using View::items;
        using View::global;
        using View::slice;

        explicit Data(Schema schema) : View(schema) {
            for (const auto& [aspectId, aspect] : schema->nodes) {
                matter().slices.emplace(aspectId, aspect.binding.createState());
            }
        }

        explicit Data(const View& source) : View(source.schema) {
            for (const auto& [aspectId, aspect] : schema->nodes) {
                matter().slices.emplace(aspectId, aspect.binding.cloneState(source));
            }
        }

        // lies a bit, better name would be "matter()"
        auto matter() -> MaterialData& { return material; }

        template<aspect::Any Meta>
        auto slice() -> TableData<Meta> { return matter().template slice<Meta>(); }

        template<aspect::Any Meta>
        auto items() -> ItemsData<Meta>& { return slice<Meta>()->items(); }

        template<aspect::Any Meta>
        auto global() -> GlobalData<Meta>& { return slice<Meta>()->global(); }

    protected:
        auto slice(aspect::Rtid runtimeTypeId) const -> cref<AbstractSlice> override { return aware_at(material.slices, runtimeTypeId); }

    private:
        MaterialData material;
        connections::World links;
    };
}