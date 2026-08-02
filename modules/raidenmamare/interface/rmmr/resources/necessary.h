#pragma once

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <fQSM/meta/rtid.h>

#include <format>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace rmmr {

    using namespace fqsm::api;

    // Unwrap maybe handle + Feature::resolve(name); refuse on miss, else invoke fn(Resolved).
    // Compiles for Assets that expose Actions::resolve → optional<Resolved>.
    template <typename Asset, typename Fn>
    auto necessary(Writing context, const base::maybe<typename Asset::Id>& handle, string name, Fn&& fn)
        -> std::optional<std::remove_cvref_t<std::invoke_result_t<Fn, const typename Asset::Resolved&>>>
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<Fn, const typename Asset::Resolved&>>;
        if (not handle) {
            (void)context.refuse(std::format("{}: handle missing", fqsm::meta::Rtid::name<Asset>()));
            return std::nullopt;
        }
        auto resolved = with<Asset>::resolve(context, *handle, name);
        if (not resolved) {
            (void)context.refuse(std::format("{}: resolve('{}') failed", fqsm::meta::Rtid::name<Asset>(), name));
            return std::nullopt;
        }
        return Result{std::invoke(std::forward<Fn>(fn), *resolved)};
    }

}
