#pragma once

#include <chrono>

namespace iqsm::q1 {
    // Q1 builtin: relative time duration, measured in seconds.
    using Seconds = double;

    // Q1 builtin: timepoint (absolute).
    using Time = std::chrono::system_clock::time_point;
}


