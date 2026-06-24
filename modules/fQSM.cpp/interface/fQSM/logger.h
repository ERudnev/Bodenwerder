#pragma once

#include <base/logging.h>
#include <fQSM/api/builtins.h>

namespace fqsm::logger {

    // Backward compatible facade. The implementation lives in `base`.
    using base::message;
    using base::fatal;
    using base::check;
    using base::report;
    using base::to_string;

    inline fqsm::q1::timepoint now() { return base::now(); }

}

namespace fqsm::logger::settings {

    namespace processing {
        static constexpr bool contextResult = true;
    }
}