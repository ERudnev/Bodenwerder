
#include <fQSM/processing/transactions/realm.h>

#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/actions/normalization.h>

namespace fqsm::processing {
    auto Realm::writing() -> Writing {
        auto patch = base::make_shared<model::complex::Patch>(reality.schema);
        auto context = std::make_shared<Context>(Context{
            reality,
            patch,
            [this](Context::Result patch) {
                accept(patch);
            }
        });

        return GateOperational{reality, context};
    }

    auto Realm::makeChildPolicy() -> ChildPolicy {
        return ChildPolicy{
            reality,
            [this](Context::Result patch) {
                accept(patch);
            }
        };
    }

    void Realm::accept(Context::Result patch) {
        lastNotes = {};
        lastNotes = actions::update(reality, *patch);
    }

    void Realm::accept_immediate(aspect::Rtid type) {
        //auto patch = model::complex::Patch(reality.schema);
        //patch.composite().slices.emplace(type, world.schema->nodes.at(type).binding.createDirtyVirtualPatch());

        lastNotes = {};
        _INCOMPLETE_; // redesing this stuff to "notify"
        //lastNotes = actions::update(reality, patch);
    }
}
