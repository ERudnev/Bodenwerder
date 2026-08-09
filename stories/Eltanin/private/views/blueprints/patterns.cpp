#include "views/blueprints/patterns.h"

#include <format>

namespace eltanin::views::blueprints::patterns {

    auto cornerMesh(mech::subframe::corner::kind kind) -> std::string_view {
        return mech::subframe::corner::specs.at(kind).code;
    }

    auto halfEdgeMesh(mech::subframe::halfEdge::kind kind, mech::subframe::halfEdge::Pole pole) -> std::string {
        const char poleTag = pole == mech::subframe::halfEdge::Pole::s ? 's' : 'e';
        return std::format("{}{}", mech::subframe::halfEdge::specs.at(kind).code, poleTag);
    }

} // namespace eltanin::views::blueprints::patterns
