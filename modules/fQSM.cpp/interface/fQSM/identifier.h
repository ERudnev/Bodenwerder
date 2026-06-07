#pragma once

#include <cstdint>
#include <format>
#include <functional>
#include <string>

namespace fqsm {

    namespace internal::id {
        using BaseType = std::uint64_t;
        extern BaseType generate_unique();
        extern std::string info_hash(BaseType); // "abc-1234" style compressed
    }

    template<typename Meta, typename BaseType = internal::id::BaseType>
    class Identifier {
    public:
        Identifier() = delete;

        using Raw = BaseType;

        explicit Identifier(BaseType v) : value(v) {}

        // do not use for any kind of logic! Only for std::map and other containers!
        bool operator<(const Identifier& rhs) const { return value < rhs.value; }
        static Identifier generate_random() { return Identifier{internal::id::generate_unique()}; }

        bool operator==(const Identifier& rhs) const { return value == rhs.value; }

        Raw raw() const { return value; }

    private:
        BaseType value;
    };

}

// Enable: std::format("{}", Identifier) -> "#abc-1234" and thus logger::message(".. {}", id);
template<typename Meta, typename BaseType>
struct std::formatter<fqsm::Identifier<Meta, BaseType>, char> : std::formatter<std::string, char> {
    template<class FormatContext>
    auto format(const fqsm::Identifier<Meta, BaseType>& id, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "#{}", fqsm::internal::id::info_hash(id.raw()));
    }
};

// Enable using Identifier as key in unordered/hamt maps (e.g. ImmutableUnorderedMap).
template<typename Meta, typename BaseType>
struct std::hash<fqsm::Identifier<Meta, BaseType>> {
    size_t operator()(const fqsm::Identifier<Meta, BaseType>& id) const noexcept {
        return std::hash<BaseType>{}(id.raw());
    }
};