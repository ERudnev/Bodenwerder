
#include <fQSM/processing/orchestrators/realm.h>

#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/algorithms/normalization.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::orchestrator {

    auto Realm::writing() -> Writing {
        auto patch = base::make_shared<model::complex::Patch>(reality.schema);
        auto context = std::make_shared<Context>(
            reality,
            patch,
            Context::Upstream{[this](Context::PatchRef patch) {
                acceptWriting(patch);
            }}
        );
        //base::message(std::format("writing() this={} op={}", (void*)this, (void*)context.get()));

        return Gate(context);
    }

    Realm::operator Stewarding() {
        auto patch = base::make_shared<model::complex::Patch>(reality.schema);
        auto session = std::make_shared<context::Synchronous>(
            reality,
            patch,
            [this](context::Synchronous::PatchRef patch, Rtid::Set dirty) {
                acceptStewarding(patch, std::move(dirty));
            }
        );
        return Dock(session);
    }

    auto Realm::makeChildPolicy() -> ChildPolicy {
        return ChildPolicy{
            View(reality),
            [this](Context::PatchRef patch) {
                acceptWriting(patch);
            }
        };
    }

    void Realm::acceptWriting(Context::PatchRef patch) {
        _DBG_TX_("realm: acceptWriting patch={}", utility::format_patch(fqsm::freeze(patch)));
        lastResult = {};
        lastResult = algorithm::update(reality, patch, {});
    }

    void Realm::acceptStewarding(Context::PatchRef patch, Rtid::Set tainted) {
        _DBG_TX_("realm: acceptStewarding patch={}", utility::format_patch(fqsm::freeze(patch)));
        lastResult = {};
        lastResult = algorithm::update(reality, patch, std::move(tainted));
    }
}
