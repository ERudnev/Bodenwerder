#pragma once

#include <cstddef>
#include <string_view>
#include <typeindex>

#include <fQSM/meta/concepts.h>

namespace fqsm::meta::aspect {

    struct Rtid {
        std::type_index value;    

        template<Any Meta>
        static Rtid of() {
            return Rtid{typeid(Meta)};
        }

        static std::string_view name(Rtid id);    

        bool operator==(const Rtid&) const = default;
        bool operator<(const Rtid& other) const {
            const auto thisHash = value.hash_code();
            const auto otherHash = other.value.hash_code();
            if (thisHash != otherHash) return thisHash < otherHash;
            return name(*this) < name(other);
        }

        struct Hash {
            auto operator()(const Rtid& id) const -> std::size_t {
                return id.value.hash_code();
            }
        };    

    private:
        explicit Rtid(std::type_index value) : value(value) {}
    };

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

}


