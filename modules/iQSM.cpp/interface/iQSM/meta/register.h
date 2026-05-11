#pragma once

#include <type_traits>

#include <iQSM/meta/concepts.h>
#include <iQSM/state/mechanism.h>
#include <iQSM/state/slice.h>

namespace iqsm::meta {
    template<Aspect Meta, state::policy::versioning VersioningType>
    struct Register : Meta {
        struct Runtime {
            using Versioning = std::integral_constant<state::policy::versioning, VersioningType>;
            using Value = state::Chunk<Meta, state::policy::order::state>;
            using Patch = state::Chunk<Meta, state::policy::order::patch>;
            using ValueSlice = state::slice::Data<Meta, Value>;

            static auto read(const Value& item) -> const iqsm::Quantum<Meta>& {
                if constexpr (Versioning::value == state::policy::versioning::shared) {
                    return *item;
                } else {
                    return item;
                }
            }
        };
    };
}
