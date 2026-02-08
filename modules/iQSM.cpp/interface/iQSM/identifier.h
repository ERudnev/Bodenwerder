#pragma once

#include <cstdint>
#include <functional>
#include <format>
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

        inline bool operator==(const Identifier& rhs) const { return value == rhs.value; }

        inline BaseType raw() const { return value; }

    private:
        BaseType value;
    };

}

// Enable: std::format("{}", Identifier) and thus logger::message(".. {}", id);
template<typename Meta, typename BaseType>
struct std::formatter<iqsm::Identifier<Meta, BaseType>, char> : std::formatter<std::string, char> {
    template<class FormatContext>
    auto format(const iqsm::Identifier<Meta, BaseType>& id, FormatContext& ctx) const {
        return std::formatter<std::string, char>::format(iqsm::internal::id::info_hash(id.raw()), ctx);
    }
};

// Enable using Identifier as key in unordered/hamt maps (e.g. ImmutableUnorderedMap).
template<typename Meta, typename BaseType>
struct std::hash<iqsm::Identifier<Meta, BaseType>> {
    size_t operator()(const iqsm::Identifier<Meta, BaseType>& id) const noexcept {
        return std::hash<BaseType>{}(id.raw());
    }
};