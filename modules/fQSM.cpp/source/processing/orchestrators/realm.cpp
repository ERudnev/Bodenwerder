#include <fQSM/processing/orchestrators/realm.h>

#include <algorithm>

#include <fQSM/erased/slots.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/processing/algorithms/normalization.h>
#include <fQSM/processing/contexts/settingUp.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::orchestrator {

    void Realm::assembleGlobals() {
        SettingUp setup(*this);
        for (model::complex::Reality::Slot slot = 0; slot < reality.schema->slotCount(); ++slot) {
            const auto assemble = reality.schema->descriptors[slot].assembleGlobal;
            if (not assemble) continue;
            struct Context {
                SettingUp& setup;
                void (*assemble)(SettingUp&, void*);
            } context{setup, assemble};
            // assembled aside: the assembling Writing may integrate into this reality meanwhile
            erased::Slots assembled(*reality.schema->descriptors[slot].global);
            assembled.push_built([](void* dst, void* raw) {
                auto& self = *static_cast<Context*>(raw);
                self.assemble(self.setup, dst);
            }, &context);
            reality.writable(slot).set_global(assembled.at(0));
        }
    }

    auto Realm::open_session(bool silent, bool direct) -> Session& {
        auto patch = base::make_shared<model::complex::Patch>(reality);
        auto& session = *open.emplace_back(std::make_unique<Session>(reality, patch, static_cast<SessionOwner*>(this), direct ? &reality : nullptr));
        session.silent = silent;
        return session;
    }

    auto Realm::writing(Mode mode) -> Writing {
        return Writing(open_session(mode == Mode::silent, false));
    }

    Realm::operator Stewarding() {
        return Stewarding(open_session(false, true));
    }

    void Realm::release(Session& session) {
        const auto found = std::find_if(open.begin(), open.end(), [&](const auto& owned) { return owned.get() == &session; });
        auto closing = std::move(*found);
        open.erase(found);
        if (closing->unwinding()) {
            // Direct writes of the session stay in the reality: they cannot be undone
            lastResult = {};
            lastResult.critical.push_back("fQSM: session discarded, an exception left its scope");
            if (not closing->silent)
                utility::log_rejected_transaction(lastResult);
            return;
        }
        accept(closing->view.patch(), std::move(closing->tainted), closing->silent);
    }

    void Realm::accept(ref<model::complex::Patch> patch, Rtid::Set tainted, bool silent) {
        _DBG_TX_("realm: accept patch={}", utility::format_patch(fqsm::freeze(patch)));
        lastResult = {};
        lastResult = algorithm::update(reality, patch, std::move(tainted));
        if (not silent)
            utility::log_rejected_transaction(lastResult);
    }
}
