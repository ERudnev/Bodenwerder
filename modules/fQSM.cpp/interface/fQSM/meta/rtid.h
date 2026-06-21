#pragma once

#include <set>
#include <cstddef>
#include <string_view>
#include <typeindex>

#include <base/runtimeTypeId.h>
#include <fQSM/meta/categories.h>

namespace fqsm::meta {

    struct Rtid {
        base::RuntimeTypeId value;

        using Set = std::set<Rtid>;

        template<category::Any Meta>
        static Rtid of() { return Rtid{typeid(Meta)}; }

        template<category::Any Meta>
        static std::string_view name() { return name(of<Meta>()); }

        static std::string_view name(Rtid id);

        bool operator==(const Rtid&) const = default;
        bool operator<(const Rtid& other) const;

        struct Hash {
            auto operator()(const Rtid& id) const -> std::size_t { return id.value.hash_code(); }
        };

    private:
        explicit Rtid(base::RuntimeTypeId value) : value(value) {}
    };

    //using TypeSet = std::set<Rtid>;
}

// impl:
namespace fqsm::meta {

    namespace detail {
        constexpr std::string_view strip_decorations(std::string_view name) {
            if (name.starts_with("struct ")) { name.remove_prefix(7); }
            else if (name.starts_with("class ")) { name.remove_prefix(6); }
            else if (name.starts_with("enum ")) { name.remove_prefix(5); }
            return name;
        }
    }

    inline std::string_view Rtid::name(Rtid id) {
        return detail::strip_decorations(id.value.name());
    }

    inline bool Rtid::operator<(const Rtid& other) const {
        const auto thisHash = value.hash_code();
        const auto otherHash = other.value.hash_code();
        if (thisHash != otherHash) return thisHash < otherHash;
        return name(*this) < name(other);
    }

}
