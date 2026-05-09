#pragma once

#include <map>
#include <optional>

#include <base/pair.h>

#include <iQSM/meta/alias.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/state/interface.h>
#include <iQSM/typeId.h>

namespace iqsm::state::policy {
    enum class versioning { // syntax: yep, small first character. enum class is not a type, it is namespace...
        shared,
        single,
    };

    enum class role {
        value,
        patch,
    };
} // policy


namespace iqsm::state::detail {
    template<meta::Aspect Meta, policy::versioning, policy::role>
    struct Representation;

    template<policy::versioning>
    struct LayerRepresentation;
}


namespace iqsm::state {
    template<meta::Aspect Meta, policy::versioning versioning, policy::role role>
    using Item = typename detail::Representation<Meta, versioning, role>::Item;

    template<policy::versioning versioning>
    using LayerFor = typename detail::LayerRepresentation<versioning>::Type;
}


namespace iqsm::state::detail {
    using TypeId = internals::Types::RuntimeId;

    template<meta::Aspect Meta>
    struct Representation<Meta, policy::versioning::shared, policy::role::value> {
        using Item = Node<Meta>;
    };

    template<meta::Aspect Meta>
    struct Representation<Meta, policy::versioning::shared, policy::role::patch> {
        using Item = base::pair<Node<Meta>>;
    };

    template<meta::Aspect Meta>
    struct Representation<Meta, policy::versioning::single, policy::role::value> {
        using Item = Quantum<Meta>;
    };

    template<meta::Aspect Meta>
    struct Representation<Meta, policy::versioning::single, policy::role::patch> {
        using Item = std::optional<Quantum<Meta>>;
    };

    template<>
    struct LayerRepresentation<policy::versioning::shared> {
        using Type = std::map<TypeId, cref<slice::Abstract>>;
    };

    template<>
    struct LayerRepresentation<policy::versioning::single> {
        using Type = std::map<TypeId, ref<slice::Abstract>>;
    };
}