#include <iQSM/logger.h>

#include <chrono>

namespace iqsm::logger {
    iqsm::q1::Time now() {
        return std::chrono::system_clock::now();
    }

    std::string to_string(iqsm::q1::Time t) {
        using namespace std::chrono;
        const auto ms = duration_cast<milliseconds>(t.time_since_epoch()).count();
        return std::to_string(ms);
    }
}


