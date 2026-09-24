#pragma once

#include <fQSM/features/_forwards.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>

namespace fqsm {
    namespace category = meta::category;
}

namespace fqsm::processing { struct SettingUp; }

// Defaults of an aspect declaration, detected in one place.
// Absent Actions: the facade is BaseActions. Absent Internals: an empty type.
// Absent customAspectReactions(): no reactions. Absent Global: an empty struct.
// Absent Always::assemble: the global is default-constructed.
namespace fqsm::meta {

    struct EmptyGlobal {};
    struct EmptyInternals {};

    namespace detail {
        template<typename M> struct FacadeOf { using type = typename M::BaseActions; };
        template<typename M> requires requires { typename M::Actions; } struct FacadeOf<M> { using type = typename M::Actions; };
        template<typename M> struct GlobalOf { using type = EmptyGlobal; };
        template<typename M> requires requires { typename M::Global; } struct GlobalOf<M> { using type = typename M::Global; };
        template<typename M> struct InternalsOf { using type = EmptyInternals; };
        template<typename M> requires requires { typename M::Internals; } struct InternalsOf<M> { using type = typename M::Internals; };
    }

    // The facade behind with<Meta>: Meta::Actions when declared, else Meta::BaseActions.
    template<typename Meta>
    using facade_t = typename detail::FacadeOf<Meta>::type;

    // Instantiate only where Meta is complete (registration, describe<Meta>()).
    template<typename Meta>
    struct aspect_info {
        using Facade = facade_t<Meta>;
        using Global = typename detail::GlobalOf<Meta>::type;
        using Internals = typename detail::InternalsOf<Meta>::type;
        static constexpr bool has_reactions = requires { Meta::customAspectReactions(); };
        static constexpr bool has_assemble = requires(processing::SettingUp& setup) { Meta::Always::assemble(setup); };

        // Behavior is complete wherever an aspect is registered (manipulation/schema.h).
        static features::Reactions reactions() {
            if constexpr (has_reactions) return Meta::customAspectReactions().rules;
            else return {};
        }
    };
}

namespace fqsm {
    template<typename Meta>
    requires meta::category::musthave::Id<Meta>
    using Id = typename Meta::Id;

    template<typename Meta>
    requires meta::category::musthave::Quantum<Meta>
    using Quantum = typename Meta::Quantum;

    template<typename Meta>
    requires meta::category::musthave::Quantum<Meta> // yes, require Quantum to allow (even empty) Global
    using GlobalValue = typename meta::detail::GlobalOf<Meta>::type;

    template<typename Meta>
    requires meta::category::Any<Meta>
    inline const meta::Rtid TypeId = meta::Rtid::of<Meta>();

}
