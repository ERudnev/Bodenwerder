#pragma once

#include <chrono>
#include <string>

namespace base {

    using Time = std::chrono::system_clock::time_point;

    Time now();
    std::string to_string(Time t);

} // namespace base
