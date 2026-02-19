#pragma once

#include <format>
#include <memory>
#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/aspects.h>
#include <iQSM/identifier.h>

namespace iqsm {

    struct FieldAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldAbstract() = default;

        // Handle type for internal plumbing (stored in World/Dag containers).
        using Ref = cref<FieldAbstract>;
    };

    template<Facet Meta>
    struct FieldObject final : Aspect<Meta>, FieldAbstract {
        using Aspect = iqsm::Aspect<Meta>;
        using Container = base::ImmutableUnorderedMap<typename Aspect::Id, typename Aspect::Item>;

        Container container;
    };

    // Handles
    template<Facet Meta>
    using Field = cref<FieldObject<Meta>>;
}

// Enable: std::format("{}", FieldObject) and thus logger::message(".. {}", field);
template<typename Meta>
struct std::formatter<iqsm::FieldObject<Meta>, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    template<class FormatContext>
    auto format(const iqsm::FieldObject<Meta>& field, FormatContext& ctx) const {
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

