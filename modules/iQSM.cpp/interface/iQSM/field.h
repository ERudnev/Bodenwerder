#pragma once

#include <format>
#include <memory>
#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/aspects.h>
#include <iQSM/identifier.h>

namespace iqsm {

    struct FieldUntyped {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldUntyped() = default;
    };

    template<Facet Meta>
    struct FieldState final : Aspect<Meta>, FieldUntyped {
        using Aspect = iqsm::Aspect<Meta>;
        using Container = base::ImmutableUnorderedMap<typename Aspect::ItemId, typename Aspect::Item>;

        Container container;
    };

    // Handles
    template<Facet Meta>
    using Field = cref<FieldState<Meta>>;
    using UField = cref<FieldUntyped>;
}

// Enable: std::format("{}", FieldState) and thus logger::message(".. {}", field);
template<typename Meta>
struct std::formatter<iqsm::FieldState<Meta>, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    template<class FormatContext>
    auto format(const iqsm::FieldState<Meta>& field, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!iqsm::Aspect<Meta>::typeName.empty()) {
            out = std::format_to(out, "{}: ", iqsm::Aspect<Meta>::typeName);
        }
        bool first = true;
        for (const auto& kv : field.container) {
            if (!first) { out = std::format_to(out, ", "); }
            first = false;
            out = std::format_to(out, "{}", kv.first);
        }
        return out;
    }
};

