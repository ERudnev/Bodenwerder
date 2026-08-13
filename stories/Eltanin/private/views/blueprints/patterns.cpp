#include "views/blueprints/patterns.h"

#include <format>

namespace eltanin::views::blueprints::patterns {

    auto cornerMesh(mech::skeleton::Corner::Kind kind) -> std::string_view {
        return mech::skeleton::cornerSpecs.at(kind).code;
    }

    auto halfEdgeMesh(mech::skeleton::Halfrib::Kind kind, mech::skeleton::Halfrib::Pole pole) -> std::string {
        const char poleTag = pole == mech::skeleton::Halfrib::Pole::starts ? 's' : 'e';
        return std::format("{}{}", mech::skeleton::halfribSpecs.at(kind).code, poleTag);
    }

} // namespace eltanin::views::blueprints::patterns
