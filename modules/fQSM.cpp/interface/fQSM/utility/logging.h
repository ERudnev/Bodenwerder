#pragma once

#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <base/logging.h>
#include <base/serialization.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/linear/patch.h>

namespace fqsm::utility {

    void log_patch(std::string_view legend, cref<model::complex::Patch> patch);

}

namespace fqsm::utility::detail {

    template<typename T>
    auto format_quantum(const T& value) -> std::string {
        return base::serialization::to_string(value);
    }

    template<category::Any Meta>
    auto format_linear_slice(const model::linear::Patch<Meta>& slice, std::string_view aspectName) -> std::string {
        std::ostringstream chain;
        bool any = false;

        if (slice.global.has_value()) {
            chain << "[global, " << format_quantum(*slice.global) << ']';
            any = true;
        }

        for (const auto entry : slice.items) {
            if (any) chain << ' ';
            chain << '[' << std::format("{}", entry.id) << ", ";
            if (!entry.value.has_value()) chain << "del";
            else chain << format_quantum(*entry.value);
            chain << ']';
            any = true;
        }

        if (!any) return {};
        return std::format("{} {}", aspectName, chain.str());
    }

    template<category::Any Meta>
    auto log_patch_slice(const model::complex::Patch& patch, std::string_view aspectName) -> std::string {
        return format_linear_slice<Meta>(patch.aspect<Meta>(), aspectName);
    }

}
