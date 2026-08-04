#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rmmr::system::content {

    // Lightweight mirror of resource::Unit::Name::{name, text} for content files /
    // tools that must not pull fQSM. Format: "library::own" (or bare "own").
    struct UnitName {
        std::string library;
        std::string own;

        auto text() const -> std::string {
            return library.empty() ? own : (library + "::" + own);
        }

        static auto name(std::string library, std::string own) -> UnitName {
            return UnitName{.library = std::move(library), .own = std::move(own)};
        }

        // Inverse of text(). Empty input or "lib::" / "::own" → nullopt.
        static auto parse(std::string_view text) -> std::optional<UnitName> {
            if (text.empty()) {
                return {};
            }
            const auto sep = text.find("::");
            if (sep == std::string_view::npos) {
                return UnitName{.library = {}, .own = std::string(text)};
            }
            if (sep == 0 or sep + 2 >= text.size()) {
                return {};
            }
            return UnitName{
                .library = std::string(text.substr(0, sep)),
                .own = std::string(text.substr(sep + 2)),
            };
        }
    };

}
