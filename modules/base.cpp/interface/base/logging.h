#pragma once

#include <iostream>
#include <string>
#include <format>

namespace base {


    inline void message(const char* msg) {
        std::cout << msg << std::endl;
    }

    inline void message(const std::string& msg) {
        message(msg.c_str());
    }

    inline void message_debug_only(const char* msg) {
        // This function is a placeholder for debug-only messages
        #ifdef DEBUG
        message(msg);
        #endif
        // In release mode, it does nothing
    }

    inline void message_debug_only(const std::string& msg) {
        message_debug_only(msg.c_str());
    }

} // namespace base
