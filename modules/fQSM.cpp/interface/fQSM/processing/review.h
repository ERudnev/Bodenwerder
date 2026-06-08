#pragma once

#include <fQSM/processing/context.h>
#include <fQSM/state/world/preview.h>

namespace fqsm::processing {

    struct Review final {
        using PatchRef = Context::PatchRef;

        state::world::Preview preview;
        PatchRef patch;

        template<aspect::Any Meta>
        auto will_be() const -> state::slice::Delta<Meta> {
            return preview.template delta<Meta>();
        }

        operator Gate() const {
            return Gate{ preview, std::make_shared<Context>(Context{preview, patch,{}}) };
        }
    };

}