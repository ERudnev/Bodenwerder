#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/features/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::erased {

    // Type-erased value operations; one static instance per C++ type (Quantum or Global).
    struct Ops {
        std::size_t size;
        std::size_t align;
        void (*construct)(void* dst);                     // nullptr when not default-constructible
        void (*copy)(void* dst, const void* src);         // copy-construct into raw storage
        void (*move)(void* dst, void* src);               // move-construct into raw storage
        void (*destroy)(void* obj);
        bool (*equal)(const void* lhs, const void* rhs);  // nullptr unless std::equality_comparable (no PFR walk in phase 1)
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

    using Category = aspect::Category;

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
        void (*groupElements)(const void* quantum, std::vector<RawId>& out);   // group only: appends the ids
        bool (*groupContains)(const void* quantum, RawId);                    // group only
        features::Reactions reactions;                     // Meta::customAspectReactions(), when declared
    };

    template<meta::category::Any Meta>
    Descriptor describe() {
        using Info = meta::aspect_info<Meta>;
        using Global = typename Info::Global;
        using Traits = typename Meta::Traits;
        Descriptor out{
            .id = TypeId<Meta>,
            .name = Rtid::name<Meta>(),
            .category = Traits::category,
            .host = std::nullopt,
            .element = std::nullopt,
            .quantum = &ops_of<Quantum<Meta>>(),
            .global = &ops_of<Global>(),
            .assembleGlobal = nullptr,
            .groupErase = nullptr,
            .groupInsert = nullptr,
            .groupElements = nullptr,
            .groupContains = nullptr,
            .reactions = {},
        };
        if constexpr (Info::has_reactions) {
            out.reactions = Meta::customAspectReactions().rules;   // Behavior is complete where aspects register
        }
        if constexpr (category::Parasitic<Meta>) {
            out.host = TypeId<typename Traits::HostAspect>;
        }
        if constexpr (category::Group<Meta>) {
            using Element = typename Traits::ElementAspect;
            out.element = TypeId<Element>;
            out.groupErase = [](void* quantum, RawId id) -> bool {
                return static_cast<Quantum<Meta>*>(quantum)->erase(Id<Element>{id}) != 0;
            };
            out.groupInsert = [](void* quantum, RawId id) {
                static_cast<Quantum<Meta>*>(quantum)->insert(Id<Element>{id});
            };
            out.groupElements = [](const void* quantum, std::vector<RawId>& ids) {
                for (const auto& id : *static_cast<const Quantum<Meta>*>(quantum))
                    ids.push_back(id.raw());
            };
            out.groupContains = [](const void* quantum, RawId id) -> bool {
                return static_cast<const Quantum<Meta>*>(quantum)->contains(Id<Element>{id});
            };
        }
        if constexpr (Info::has_assemble) {
            out.assembleGlobal = [](SettingUp& setup, void* dst) {
                ::new (dst) Global(Meta::Always::assemble(setup));
            };
        } else {
            static_assert(std::is_default_constructible_v<Global>, "fQSM: Global is not default-constructible; declare always >assemble() -> all");
        }
        return out;
    }
}
