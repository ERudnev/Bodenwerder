#pragma once

#include <base/shared_reference.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/state/schema.h>
#include <fQSM/state/patch.h>
#include <fQSM/state/slice.h>
#include <fQSM/state/world.h>

namespace fqsm::state::details {
    namespace axis = meta::axis;
    namespace aspect = meta::aspect;

    template<aspect::Any Meta>
    auto makeSliceFactory() -> SchemaData::SliceFactory {
        return {
            []() -> ref<slice::Abstract<axis::order::state>> {
                return base::make_shared<slice::Data<Meta, axis::order::state>>();
            },
            [](const world::View& source) -> ref<slice::Abstract<axis::order::state>> {
                auto out = base::make_shared<slice::Data<Meta, axis::order::state>>();
                for (const auto entry : source.template items<Meta>()) {
                    out->items().insert(entry.first, entry.second);
                }
                return out;
            },
            [](const world::View& state, const world::Patch& patch) -> ref<slice::Abstract<axis::order::state>> {
                return base::make_shared<slice::Overlay<Meta>>(state.template slice<Meta>(), patch.template slice<Meta>());
            },
        };
    }
}
