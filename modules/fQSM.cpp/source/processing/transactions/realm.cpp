
#include <fQSM/processing/transactions/realm.h>

#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/actions/normalization.h>

namespace fqsm::processing {

    auto Realm::writing() -> Writing {
        auto patch = base::make_shared<model::complex::Patch>(reality.schema);
        auto context = std::make_shared<Context>(Context{
            reality,
            patch,
            [this](Context::PatchRef patch) {
                accept(patch);
            }
        });

        return Gate(context);
    }

    auto Realm::makeChildPolicy() -> ChildPolicy {
        return ChildPolicy{
            reality,
            [this](Context::PatchRef patch) {
                accept(patch);
            }
        };
    }

    void Realm::accept(Context::PatchRef patch) {
        lastNotes = {};
        lastNotes = actions::update(reality, *patch);
    }

    void Realm::accept_immediate(Rtid::Set affected) {
        //auto patch = model::complex::Patch(reality.schema);
        //patch.composite().slices.emplace(type, world.schema->nodes.at(type).binding.createDirtyVirtualPatch());

        lastNotes = {};
        if (affected.empty()) return;
        _INCOMPLETE_; // redesing this stuff to "notify"
        //lastNotes = actions::update(reality, patch);
    }
}
