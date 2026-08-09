#pragma once

#include "mech/semantics/subframe.h"

#include <string>
#include <string_view>

namespace eltanin::views::blueprints::patterns {

    auto cornerMesh(mech::subframe::corner::kind kind) -> std::string_view;
    auto halfEdgeMesh(mech::subframe::halfEdge::kind kind, mech::subframe::halfEdge::Pole pole) -> std::string;

} // namespace eltanin::views::blueprints::patterns
