#pragma once

#include <type_traits>
#include <format>
#include <memory>
#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/_forwards.h>
#include <iQSM/meta.h>
#include <iQSM/identifier.h>

namespace iqsm {

    struct FieldAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldAbstract() = default;

        // Handle type for internal plumbing (stored in World/Dag containers).
        using Ref = cref<FieldAbstract>;
    };

    template<meta::Aspect Meta>
    struct FieldObject final : Facet<Meta>, FieldAbstract {
        using Facet = iqsm::Facet<Meta>;
        using GlobalData = typename Facet::GlobalData;
        using Global = typename Facet::Global;
        using Container = base::ImmutableUnorderedMap<typename Facet::Id, typename Facet::Item>;

        Container container;
        Global global = [] { static const Global zeroGlobal = base::make_shared<const GlobalData>(); return zeroGlobal; }();
    };

    // Handles
    template<meta::Aspect Meta>
    using Field = cref<FieldObject<Meta>>;
}

// Enable: std::format("{}", FieldObject) and thus logger::message(".. {}", field);
template<typename Meta>
struct std::formatter<iqsm::FieldObject<Meta>, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    template<class FormatContext>
    auto format(const iqsm::FieldObject<Meta>& field, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!iqsm::Facet<Meta>::typeName.empty()) {
            out = std::format_to(out, "{}: ", iqsm::Facet<Meta>::typeName);
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

