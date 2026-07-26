#include <fQSM/processing/orchestrators/realm_safe.h>

#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/processing/algorithms/normalization.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::orchestrator {

    void RealmSafe::crash_if_deferred() const {
        if (deferred_writing or deferred_stewarding)
            std::abort();
    }

    auto RealmSafe::writing(Mode mode) -> Writing {
        auto patch = base::make_shared<model::complex::Patch>(reality.schema);
        auto context = std::make_shared<Context>(
            reality,
            patch,
            Context::Upstream{[this, mode](Context::PatchRef patch) {
                acceptWriting(patch, mode);
            }}
        );
        return Gate(context);
    }

    RealmSafe::operator Stewarding() {
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

    auto RealmSafe::makeChildPolicy() -> ChildPolicy {
        return ChildPolicy{
            View(reality),
            [this](Context::PatchRef patch) {
                acceptWriting(patch, Mode::normal);
            }
        };
    }

    void RealmSafe::acceptWriting(Context::PatchRef patch, Mode mode) {
        crash_if_deferred();
        deferred_writing = DeferredWriting{
            .patch = patch.std_ptr(),
            .mode = mode,
        };
    }

    void RealmSafe::acceptStewarding(Context::PatchRef patch, Rtid::Set tainted) {
        crash_if_deferred();
        deferred_stewarding = DeferredStewarding{
            .patch = patch.std_ptr(),
            .tainted = std::move(tainted),
        };
    }

    void RealmSafe::finish_patch() {
        if (deferred_writing) {
            auto parked = std::move(*deferred_writing);
            deferred_writing.reset();
            Context::PatchRef patch{parked.patch};
            _DBG_TX_("realm_safe: finish_patch writing={}", utility::format_patch(fqsm::freeze(patch)));
            lastResult = {};
            lastResult = algorithm::update(reality, patch, {});
            if (parked.mode != Mode::silent)
                utility::log_rejected_transaction(lastResult);
            return;
        }
        if (deferred_stewarding) {
            auto parked = std::move(*deferred_stewarding);
            deferred_stewarding.reset();
            Context::PatchRef patch{parked.patch};
            _DBG_TX_("realm_safe: finish_patch stewarding={}", utility::format_patch(fqsm::freeze(patch)));
            lastResult = {};
            lastResult = algorithm::update(reality, patch, std::move(parked.tainted));
            utility::log_rejected_transaction(lastResult);
            return;
        }
        std::abort();
    }
}
