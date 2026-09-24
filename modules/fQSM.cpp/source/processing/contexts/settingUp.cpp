#include <fQSM/processing/contexts/settingUp.h>

#include <fQSM/processing/transaction.h>

namespace fqsm::processing {

    auto SettingUp::writing() -> Writing {
        return transaction.writing(Transaction::Mode::normal);
    }

}
