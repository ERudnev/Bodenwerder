#pragma once

#include "mech/semantics/subframe.h"

#include <string>
#include <string_view>

namespace eltanin::views::blueprints::patterns {

    auto cornerMesh(mech::skeleton::Corner::Kind kind) -> std::string_view;
    auto halfEdgeMesh(mech::skeleton::Halfrib::Kind kind, mech::skeleton::Halfrib::Pole pole) -> std::string;

} // namespace eltanin::views::blueprints::patterns
