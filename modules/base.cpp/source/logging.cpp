#include <base/logging.h>
#include <base/clock.h>

#include <chrono>
#include <ctime>
#include <exception>
#include <format>
#include <iostream>

namespace base {

    void message(std::string_view msg) {
        std::cout << msg << std::endl;
    }

    void vmessage(std::string_view fmt, std::format_args args) {
        message(std::vformat(fmt, args));
    }

    Progress* Progress::current = nullptr;

    Progress::Progress(std::string_view label) {
        current = this;
        std::cout << label << ' ' << std::flush;
    }

    void Progress::tick() {
        std::cout << '.' << std::flush;
    }

    Progress::~Progress() {
        std::cout << std::endl;
        if (current == this)
            current = nullptr;
    }

    void Progress::mark() {
        if (current)
            current->tick();
    }

    void Progress::markEvery(long long index) {
        if (current and (index & 0x1fffff) == 0)
            current->tick();
    }

    void whisper(std::string_view msg) {
        std::cout << "\033[90m" << msg << "\033[0m" << std::endl;
    }

    void vwhisper(std::string_view fmt, std::format_args args) {
        whisper(std::vformat(fmt, args));
    }

    void warning(std::string_view msg) {
        std::cout << "\033[33m" << msg << "\033[0m" << std::endl;
    }

    void vwarning(std::string_view fmt, std::format_args args) {
        warning(std::vformat(fmt, args));
    }

    void fatal(std::string_view msg) {
        message("[{}] FATAL: {}", to_string(now()), msg);
        std::terminate();
    }

    void vfatal(std::string_view fmt, std::format_args args) {
        try {
            fatal(std::vformat(fmt, args));
        } catch (const std::format_error& e) {
            fatal(std::string("format error: ") + e.what());
        } catch (...) {
            fatal("unknown formatting error");
        }
    }

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
