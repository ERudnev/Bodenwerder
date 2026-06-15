#pragma once

#include <base/shared_reference.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/state/composite.h>
#include <fQSM/state/slice/delta.h>
#include <fQSM/state/slice/draft.h>
#include <fQSM/state/world/actual.h>
#include <fQSM/state/world/patch.h>

namespace fqsm::state::world {

    struct Draft : Composite<slice::Draft> {
        const Actual& actual;
        Patch& patch;

        Draft(const Actual& actual, Patch& patch)
            : Composite<slice::Draft>(actual.schema)
            , actual(actual)
            , patch(patch)
        {}

        template<aspect::Any Meta>
        auto slice() const -> cref<slice::Draft<Meta>> {
            ensure<Meta>();
            return Composite<slice::Draft>::slice<Meta>();
        }

        template<aspect::Any Meta>
        auto delta(typename slice::Delta<Meta>::StateInterpretation interpretation) const
            -> slice::Delta<Meta>
        {
            return slice::Delta<Meta>{actual.slice<Meta>(), patch.slice<Meta>(), interpretation};
        }

        template<aspect::Any Meta>
        auto delta() const -> slice::Delta<Meta> {
            return delta<Meta>(slice::Delta<Meta>::StateInterpretation::clean);
        }

    private:
        template<aspect::Any Meta>
        void ensure() const {
            const auto id = aspect::Rtid::of<Meta>();
            if (slices.contains(id)) {
                return;
            }

            const_cast<Draft*>(this)->slices.emplace(
                id,
                base::make_shared<slice::Draft<Meta>>(actual.slice<Meta>(), patch.slice<Meta>())
            );
        }
    };

}
