#pragma once

#include <cstdint>
#include <string>

namespace iqsm {

    namespace internal::id {
        using BaseType = std::uint64_t;
        extern BaseType generate_unique();
        extern std::string info_hash(BaseType); // "abc-1234" style compressed
    }

    template<typename Meta, typename BaseType = internal::id::BaseType>
    class Identifier {
    public:
        Identifier() = delete;

        explicit Identifier(BaseType v) : value(v) {}

        // do not use for any kind of logic! Only for std::map and other containers!
        inline bool operator<(const Identifier& rhs) const { return value < rhs.value; }
        //remove? Identifier advance() const { return {identifiers::advance(value)}; } // will not compile with non-integers. Redo this
        static Identifier generate_random() { return Identifier{internal::id::generate_unique()}; }
        inline std::string debug_string() const { return internal::id::info_hash(value); }

        inline bool operator==(const Identifier& rhs) const { return value == rhs.value; }

        inline BaseType raw() const { return value; }

    private:
        BaseType value;
    };

}