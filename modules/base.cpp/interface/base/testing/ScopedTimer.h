#pragma once
#include <chrono>
#include <iostream>

namespace testing {

    struct scoped_timer
    {
        scoped_timer(const char* name);
        ~scoped_timer();

        const char* name() const { return name_; }
        std::string finish();
        int elapsed_us() const;
    private:
        const char* name_;
        bool armed = true;
        std::chrono::high_resolution_clock::time_point start_time_;

        std::string current_message() const;
    };
}
