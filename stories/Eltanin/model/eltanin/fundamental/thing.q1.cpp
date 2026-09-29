#include <eltanin/fundamental/thing.q1.h>

#include <algorithm>
#include <cmath>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    namespace {

        constexpr int64 secondsPerMinute = 60;
        constexpr int64 secondsPerHour = 60 * secondsPerMinute;
        constexpr int64 secondsPerDay = 24 * secondsPerHour;
        constexpr int64 secondsPerMonth = 30 * secondsPerDay;
        constexpr int64 secondsPerYear = 12 * secondsPerMonth;

    }

    auto Thing::Always::setup(SettingUp&) -> Thing::Global {
        return Global{.now = seconds{}};
    }

    auto Thing::Always::civil(seconds now) -> Civil {
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

    void Thing::Actions::update(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        with<Thing>::modify_global(context)->now += dt;
    }

    auto doctrine::thing() -> Schema {
        return ask::schema::aspect<Thing>();
    }

}
