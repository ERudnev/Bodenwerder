#include "views/blueprints/patterns.h"

#include <format>

namespace eltanin::views::blueprints::patterns {

    void apply(mech::Element::Cell& cell) {
        const auto found = mech::subframe::recipes.find(cell.shape);
        if (found == mech::subframe::recipes.end()) {
            cell.corners = {};
            cell.edges = {};
            return;
        }
        cell.corners = found->second.corners;
        cell.edges = found->second.edges;
    }

    auto cornerMesh(mech::subframe::corner::kind kind) -> std::string_view {
        using K = mech::subframe::corner::kind;
        switch (kind) {
            case K::c124: return "c124";
            case K::c1364: return "c1364";
            case K::c164: return "c164";
            case K::c134: return "c134";
            case K::c135: return "c135";
            case K::c12: return "c12";
            case K::c13: return "c13";
            case K::c15: return "c15";
            case K::c16: return "c16";
            case K::c34: return "c34";
            case K::c35: return "c35";
        }
        return {};
    }

    auto halfEdgeMesh(mech::subframe::halfEdge::kind kind, mech::subframe::halfEdge::Pole pole) -> std::string {
        namespace he = mech::subframe::halfEdge;
        const auto& spec = he::specs.at(kind);
        const char poleTag = pole == he::Pole::s ? 's' : 'e';
        if (kind == he::kind::he1deg90 and pole == he::Pole::s)
            return std::format("he1ged90{}", poleTag);
        return std::format("{}{}", spec.code, poleTag);
    }

} // namespace eltanin::views::blueprints::patterns
