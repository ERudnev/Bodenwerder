
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
        lastNotes = actions::update(reality, *patch, {});
    }

    void Realm::accept_immediate(Rtid::Set affected) {
        lastNotes = {}; // reset stored "error buffer"
        if (affected.empty()) return;
        // TODO: make this as combined PAtch+taint later...
        auto zeroPatch = std::make_shared<Patch>(reality.schema);
        lastNotes = actions::update(reality, *zeroPatch, affected);
;
    }
}
