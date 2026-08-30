#include <fQSM/processing/contexts/settingUp.h>

#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing {

    SettingUp::SettingUp(Transaction& transaction, model::complex::Reality& reality)
        : transaction(transaction)
        , reality(reality)
    {}

    auto SettingUp::writing() -> Writing {
        return transaction.writing(Transaction::Mode::normal);
    }

    void SettingUp::emplace(meta::Rtid typeId, ref<model::linear::state::Erased> line) {
        reality.putLine(typeId, std::move(line));
    }

}
