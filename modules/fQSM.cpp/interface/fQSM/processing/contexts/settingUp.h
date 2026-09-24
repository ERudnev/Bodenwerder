#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    // Context of Always::assemble at Realm birth: gives a Writing into the Realm being built.
    struct SettingUp {
        friend struct orchestrator::Realm;

        SettingUp(const SettingUp&) = delete;
        SettingUp& operator=(const SettingUp&) = delete;

        auto writing() -> Writing;

    private:
        explicit SettingUp(Transaction& transaction) : transaction(transaction) {}
        Transaction& transaction;
    };
}
