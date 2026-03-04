#pragma once
// this is NOT "generated" C++ file, this is example of user (project) wrapper for Q1-generated type of resource
// intended as this kind of files are dwelled withing generated library of Aspects ad pair-entities for Resources

// hint: #include for Aspect (Resource) TextDescription is required to access Resource::Passport as "fabric parameters"
#include <Atomic/varph.q1.h>

#include <cstring>
#include <string>

#include <iQSM/resources.h>

namespace Q1CORE::Example::Varph::handlers {

    struct TextDescription {
        using Passport = Varph::TextDescription::Passport;

        static std::string make_name(const Passport& passport) {
            std::string out = passport.fileName;
            if (const auto slash = out.find_last_of("/\\"); slash != std::string::npos) {
                out.erase(0, slash + 1);
            }
            if (const auto dot = out.find_last_of('.'); dot != std::string::npos) {
                out.erase(dot);
            }
            return out;
        }

        explicit TextDescription(const Passport& passport)
            : name(make_name(passport)) {
            const auto s = std::string{"mock:"} + passport.fileName;
            buffer = new char[s.size() + 1];
            std::memcpy(buffer, s.c_str(), s.size() + 1);
        }

        ~TextDescription() {
            delete[] buffer;
        }

        TextDescription(const TextDescription&) = delete;
        TextDescription& operator=(const TextDescription&) = delete;

        TextDescription(TextDescription&&) = delete;
        TextDescription& operator=(TextDescription&&) = delete;

        const char* c_str() const {
            return buffer ? buffer : "";
        }

        const std::string name;
        char* buffer = nullptr;
    };
}

namespace iqsm::detail::resources {
    template<>
    struct handler_of<Q1CORE::Example::Varph::TextDescription> {
        using type = Q1CORE::Example::Varph::handlers::TextDescription;
    };
}

