#include <eltanin/fundamental/existent.q1.h>

#include <algorithm>
#include <cmath>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    namespace {

        constexpr seconds warpRates[] = {
            seconds{0},
            seconds{1},
            seconds{5},
            seconds{10},
            seconds{50},
            seconds{100},
            seconds{1000},
            seconds{10000},
            seconds{100000},
        };
        constexpr const char* warpLabels[] = {
            "Pause",
            "×1",
            "×5",
            "×10",
            "×50",
            "×100",
            "×1 000",
            "×10 000",
            "×100 000",
        };
        constexpr int64 secondsPerMinute = 60;
        constexpr int64 secondsPerHour = 60 * secondsPerMinute;
        constexpr int64 secondsPerDay = 24 * secondsPerHour;
        constexpr int64 secondsPerMonth = 30 * secondsPerDay;
        constexpr int64 secondsPerYear = 12 * secondsPerMonth;

        auto clampWarp(integer warp) -> integer {
            return std::clamp(warp, integer{0}, Existent::Always::warpTop);
        }

    }

    auto Existent::Always::assemble(SettingUp&) -> Existent::Global {
        return Global{.now = seconds{}, .warp = 1};
    }

    auto Existent::Always::rate(integer warp) -> seconds {
        return warpRates[static_cast<std::size_t>(clampWarp(warp))];
    }

    auto Existent::Always::civil(seconds now) -> Civil {
        int64 t = static_cast<int64>(std::floor(std::max(now, seconds{0})));
        const integer year = static_cast<integer>(t / secondsPerYear + 1);
        t %= secondsPerYear;
        const integer month = static_cast<integer>(t / secondsPerMonth + 1);
        t %= secondsPerMonth;
        const integer day = static_cast<integer>(t / secondsPerDay + 1);
        t %= secondsPerDay;
        const integer hour = static_cast<integer>(t / secondsPerHour);
        t %= secondsPerHour;
        const integer minute = static_cast<integer>(t / secondsPerMinute);
        const integer second = static_cast<integer>(t % secondsPerMinute);
        return Civil{.year = year, .month = month, .day = day, .hour = hour, .minute = minute, .second = second};
    }

    auto Existent::Always::warpLabel(integer warp) -> const char* {
        return warpLabels[static_cast<std::size_t>(clampWarp(warp))];
    }

    void Existent::Actions::update(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        auto existent = with<Existent>::modify_global(context);
        const seconds scaled = dt * Always::rate(existent->warp);
        if (scaled <= 0)
            return;
        existent->now += scaled;
    }

    auto doctrine::existent() -> Schema {
        return ask::schema::aspect<Existent>();
    }

}
