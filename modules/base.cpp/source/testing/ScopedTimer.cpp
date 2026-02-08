#include <base/testing/ScopedTimer.h>
// Remove unnecessary includes
#include <base/logging.h>

namespace testing {

    scoped_timer::scoped_timer(const char* name)
        : name_(name), start_time_(std::chrono::high_resolution_clock::now())
    {
        // Timer starts upon construction
    }

    scoped_timer::~scoped_timer()
    {
        if (armed) {
            base::message(current_message());
        }
    }

    int scoped_timer::elapsed_us() const
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        return static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_).count());
    }

    std::string scoped_timer::finish()
    {
        if (armed) {
            armed = false; // Prevent double completion
            return current_message();
        }
        else {
            return std::format("Timer [{}] already completed.", name_);
        }
    }

    std::string scoped_timer::current_message() const
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_).count();

        // Convert microseconds to milliseconds with decimal part
        double duration_ms = duration_us / 1000.0;

        return std::format("timer [{}] = {:.3f} ms", name_, duration_ms);
    }
}
