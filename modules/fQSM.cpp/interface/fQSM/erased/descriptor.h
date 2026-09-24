#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fQSM/identifier.h>
#include <fQSM/features/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::aspect {
    template<typename Meta> struct Entity;
    template<typename Meta, typename Host> struct Attribute;
    template<typename Meta, typename Host> struct Feature;
    template<typename Meta, typename Host> struct Component;
    template<typename Meta, typename Host, typename Worker> struct Group;
}

namespace fqsm::erased {

    // Type-erased value operations; one static instance per C++ type (Quantum or Global).
    struct Ops {
        std::size_t size;
        std::size_t align;
        void (*construct)(void* dst);                     // nullptr when not default-constructible
        void (*copy)(void* dst, const void* src);         // copy-construct into raw storage
        void (*move)(void* dst, void* src);               // move-construct into raw storage
        void (*destroy)(void* obj);
        bool (*equal)(const void* lhs, const void* rhs);  // nullptr when not equality-comparable
    };

    namespace detail {
        template<typename T>
        struct OpsOf {
            static void construct(void* dst) { ::new (dst) T(); }
            static void copy(void* dst, const void* src) { ::new (dst) T(*static_cast<const T*>(src)); }
            static void move(void* dst, void* src) { ::new (dst) T(std::move(*static_cast<T*>(src))); }
            static void destroy(void* obj) { static_cast<T*>(obj)->~T(); }
            static bool equal(const void* lhs, const void* rhs) {
                return *static_cast<const T*>(lhs) == *static_cast<const T*>(rhs);
            }
        };
    }

    template<typename T>
    const Ops& ops_of() {
        static_assert(std::is_copy_constructible_v<T> and std::is_move_constructible_v<T> and std::is_destructible_v<T>,
            "fqsm::erased::ops_of: type must be copyable, movable and destructible");
        using Of = detail::OpsOf<T>;
        static constexpr Ops ops = [] {
            Ops out{sizeof(T), alignof(T), nullptr, &Of::copy, &Of::move, &Of::destroy, nullptr};
            if constexpr (std::is_default_constructible_v<T>) out.construct = &Of::construct;
            if constexpr (std::equality_comparable<T>) out.equal = &Of::equal;
            return out;
        }();
        return ops;
    }

    enum class Category : std::uint8_t { entity, attribute, feature, component, group };

    struct Descriptor {
        Rtid id;
        std::string_view name;
        Category category;
        std::optional<Rtid> host;                          // parasitic categories
        std::optional<Rtid> element;                       // group: worker aspect
        const Ops* quantum;
        const Ops* global;
        void (*assembleGlobal)(SettingUp&, void* dst);     // constructs into raw storage; nullptr when Global is default-constructible
        bool (*groupErase)(void* quantum, RawId);          // group only
        void (*groupInsert)(void* quantum, RawId);         // group only
        features::Reactions reactions;                     // DefaultInternals::reactions()
    };

    namespace detail {
        template<typename Meta>
        concept HasGlobalAssemble = requires (SettingUp& setup) {
            { Meta::Always::assemble(setup) } -> std::same_as<GlobalValue<Meta>>;
        };

        template<typename Meta>
        constexpr Category category_of() {
            if constexpr (std::is_base_of_v<aspect::Entity<Meta>, Meta>) {
                return Category::entity;
            } else if constexpr (category::Group<Meta>) {
                return Category::group;
            } else if constexpr (std::is_base_of_v<aspect::Attribute<Meta, typename Meta::HostAspect>, Meta>) {
                return Category::attribute;
            } else if constexpr (std::is_base_of_v<aspect::Feature<Meta, typename Meta::HostAspect>, Meta>) {
                return Category::feature;
            } else {
                static_assert(std::is_base_of_v<aspect::Component<Meta, typename Meta::HostAspect>, Meta>,
                    "fqsm::erased::describe: unknown aspect category");
                return Category::component;
            }
        }
    }

    template<meta::category::Any Meta>
    Descriptor describe() {
        using Global = GlobalValue<Meta>;
        Descriptor out{
            .id = TypeId<Meta>,
            .name = Rtid::name<Meta>(),
            .category = detail::category_of<Meta>(),
            .host = std::nullopt,
            .element = std::nullopt,
            .quantum = &ops_of<Quantum<Meta>>(),
            .global = &ops_of<Global>(),
            .assembleGlobal = nullptr,
            .groupErase = nullptr,
            .groupInsert = nullptr,
            .reactions = Meta::DefaultInternals::reactions().rules,
        };
        if constexpr (category::Parasitic<Meta>) {
            out.host = TypeId<typename Meta::HostAspect>;
        }
        if constexpr (category::Group<Meta>) {
            using Element = typename Meta::WorkerAspect;
            out.element = TypeId<Element>;
            out.groupErase = [](void* quantum, RawId id) -> bool {
                return static_cast<Quantum<Meta>*>(quantum)->erase(Id<Element>{id}) != 0;
            };
            out.groupInsert = [](void* quantum, RawId id) {
                static_cast<Quantum<Meta>*>(quantum)->insert(Id<Element>{id});
            };
        }
        if constexpr (detail::HasGlobalAssemble<Meta>) {
            out.assembleGlobal = [](SettingUp& setup, void* dst) {
                ::new (dst) Global(Meta::Always::assemble(setup));
            };
        } else {
            static_assert(std::is_default_constructible_v<Global>, "fQSM: Global is not default-constructible; declare always >assemble() -> all");
        }
        return out;
    }
}
