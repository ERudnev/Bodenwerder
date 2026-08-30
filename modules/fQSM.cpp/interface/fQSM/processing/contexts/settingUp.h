#pragma once

#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/references.h>

namespace fqsm::processing {

    struct Transaction;
    namespace orchestrator { struct Realm; struct RealmSafe; }

    struct SettingUp {
        friend struct orchestrator::Realm;
        friend struct orchestrator::RealmSafe;

        SettingUp(const SettingUp&) = delete;
        SettingUp& operator=(const SettingUp&) = delete;
        SettingUp(SettingUp&&) = delete;
        SettingUp& operator=(SettingUp&&) = delete;

        auto writing() -> Writing;
        void emplace(meta::Rtid typeId, ref<model::linear::state::Erased> line);

    private:
        explicit SettingUp(Transaction& transaction, model::complex::Reality& reality);
        Transaction& transaction;
        model::complex::Reality& reality;
    };
}

namespace fqsm {
    using SettingUp = processing::SettingUp;
}
