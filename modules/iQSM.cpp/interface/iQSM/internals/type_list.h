#pragma once

#include <type_traits>

namespace iqsm::internals {
    template<typename... Ts>
    struct type_list {};

    template<typename T, typename List>
    struct tl_contains;

    template<typename T, typename... Ts>
    struct tl_contains<T, type_list<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

    template<typename List, typename T>
    struct tl_push_unique;

    template<typename... Ts, typename T>
    struct tl_push_unique<type_list<Ts...>, T> {
        using type = std::conditional_t<tl_contains<T, type_list<Ts...>>::value, type_list<Ts...>, type_list<Ts..., T>>;
    };

    template<typename Accum, typename AddList>
    struct tl_append_list_unique;

    template<typename Accum>
    struct tl_append_list_unique<Accum, type_list<>> { using type = Accum; };

    template<typename Accum, typename T, typename... Rest>
    struct tl_append_list_unique<Accum, type_list<T, Rest...>> {
        using next = typename tl_push_unique<Accum, T>::type;
        using type = typename tl_append_list_unique<next, type_list<Rest...>>::type;
    };

    template<typename List, typename Accum>
    struct tl_filter_missing;

    template<typename Accum>
    struct tl_filter_missing<type_list<>, Accum> { using type = type_list<>; };

    template<typename T, typename... Rest, typename Accum>
    struct tl_filter_missing<type_list<T, Rest...>, Accum> {
        using tail = typename tl_filter_missing<type_list<Rest...>, Accum>::type;
        using type = std::conditional_t<tl_contains<T, Accum>::value, tail, typename tl_push_unique<tail, T>::type>;
    };

    template<typename Pending, typename Accum>
    struct tl_closure;

    template<typename Accum>
    struct tl_closure<type_list<>, Accum> { using type = Accum; };

    template<typename T, typename = void>
    struct deps_of { using type = type_list<>; };

    template<typename T>
    struct deps_of<T, std::void_t<typename T::Depends>> { using type = typename T::Depends; };

    template<typename Head, typename... Tail, typename Accum>
    struct tl_closure<type_list<Head, Tail...>, Accum> {
        using deps = typename deps_of<Head>::type;
        using deps_missing = typename tl_filter_missing<deps, Accum>::type;
        using next_pending = typename tl_append_list_unique<type_list<Tail...>, deps_missing>::type;
        using next_accum = typename tl_append_list_unique<typename tl_push_unique<Accum, Head>::type, deps>::type;
        using type = typename tl_closure<next_pending, next_accum>::type;
    };
}

