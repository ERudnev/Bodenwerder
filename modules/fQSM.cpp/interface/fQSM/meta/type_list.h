#pragma once

#include <type_traits>

namespace fqsm::meta::internals {
    template<typename... Metas>
    struct type_list {};

    template<typename Meta, typename List>
    struct tl_contains;

    template<typename Meta, typename... Metas>
    struct tl_contains<Meta, type_list<Metas...>> : std::bool_constant<(std::is_same_v<Meta, Metas> || ...)> {};

    template<typename List, typename Meta>
    struct tl_push_unique;

    template<typename... Metas, typename Meta>
    struct tl_push_unique<type_list<Metas...>, Meta> {
        using type = std::conditional_t<tl_contains<Meta, type_list<Metas...>>::value, type_list<Metas...>, type_list<Metas..., Meta>>;
    };

    template<typename Accumulated, typename AddedList>
    struct tl_append_list_unique;

    template<typename Accumulated>
    struct tl_append_list_unique<Accumulated, type_list<>> {
        using type = Accumulated;
    };

    template<typename Accumulated, typename Meta, typename... RestMetas>
    struct tl_append_list_unique<Accumulated, type_list<Meta, RestMetas...>> {
        using next = typename tl_push_unique<Accumulated, Meta>::type;
        using type = typename tl_append_list_unique<next, type_list<RestMetas...>>::type;
    };

    template<typename Meta, typename = void>
    struct explicit_deps_of {
        using type = type_list<>;
    };

    template<typename Meta>
    struct explicit_deps_of<Meta, std::void_t<typename Meta::Depends>> {
        using type = typename Meta::Depends;
    };

    template<typename Meta, typename = void>
    struct parent_deps_of {
        using type = type_list<>;
    };

    template<typename Meta>
    struct parent_deps_of<Meta, std::void_t<typename Meta::ParentAspect>> {
        using type = type_list<typename Meta::ParentAspect>;
    };
}

namespace fqsm::meta {
    template<typename Meta>
    struct deps_of {
        using type = typename internals::tl_append_list_unique<typename internals::parent_deps_of<Meta>::type, typename internals::explicit_deps_of<Meta>::type>::type;
    };
}
