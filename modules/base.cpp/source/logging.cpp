#include <base/logging.h>

#include <chrono>
#include <ctime>
#include <format>

namespace base {
    Time now() {
        return std::chrono::system_clock::now();
    }

    std::string to_string(Time t) {
        using namespace std::chrono;

        const auto tt = system_clock::to_time_t(t);
        std::tm local_tm{};
#if defined(_WIN32)
        localtime_s(&local_tm, &tt);
#else
        localtime_r(&tt, &local_tm);
#endif

        const auto t_ms = duration_cast<milliseconds>(t.time_since_epoch());
        const auto ms_part = static_cast<int>(t_ms.count() % 1000);

        return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
            local_tm.tm_year + 1900,
            local_tm.tm_mon + 1,
            local_tm.tm_mday,
            local_tm.tm_hour,
            local_tm.tm_min,
            local_tm.tm_sec,
            ms_part
        );
    }
}


