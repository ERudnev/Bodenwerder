#pragma once

#include "shapes.h"

#include <vector>

#include <base/types/common_types.h>
#include <eltanin/mech/semantics.q1.h>
#include <rmmr/renderer/types.q1.h>

#include <glm/common.hpp>

namespace eltanin::mech::space {

    using base::common_types::ivec3;
    using base::common_types::imat3;
    using base::common_types::mat3;
    using base::common_types::vec3;

    // Floating-point body / world meters for any object.
    namespace local {
        // Cell edge length in meters (scale between discrete lattices and local).
        inline constexpr float edge2meters = 4.0f;
        using point = vec3;
    }

    // Discrete cell lattice ({0,1}³ inside a cell; cell indices in the construction lattice).
    // Frame quarks author here (skeleton::Placement). Mounts use space::Transform (grid space) instead.
    namespace cell {
        using index = ivec3;
        using Placement = ::eltanin::mech::skeleton::Placement;

        // Lattice integer coords (typically cube::corners {0,1}³) → meters, cell-centered.
        inline auto cell2local(cell::index lattice) -> local::point { return (local::point(lattice) - local::point{0.5f}) * local::edge2meters; }

        // Inverse of cell2local on the lattice image (round for float noise).
        inline auto local2cell(local::point meters) -> index { return index(glm::round(meters / local::edge2meters + local::point{0.5f})); }

        // World meters of a cell's center (cell i spans continuous [i, i+1] on each axis).
        inline auto center2local(index cell) -> local::point { return (local::point(cell) + local::point{0.5f}) * local::edge2meters; }

    } // namespace cell

    // Cube orientation alphabet (RedStar CubicRotation). Key 0..23; 0 = identity.
    // matrix[i] — 3×3 in {-1,0,1}; listed by rows (see space.cpp).
    // turn(Semiaxis) — after ±90° about local axis: new key = turn(axis)[old].
    // key — primary alias in doctrine/model (semantics.q1); not redefined here.
    namespace orient {

        enum class Semiaxis { Xp, Xn, Yp, Yn, Zp, Zn, };

        extern const std::vector<imat3> matrix;
        extern const std::vector<key> invert;
        // compose[outer][inner] — world = outer · inner (column vectors).
        extern const std::vector<std::vector<key>> compose;

        auto turn(Semiaxis) -> const std::vector<key>&;

        // Body-local meters: R · cell::cell2local(lattice).
        auto cell2local(key orientation, cell::index lattice) -> vec3;

        // Lattice Corner 0..7 after orientation ({0,1} ↔ {−1,+1} round-trip, exact on the group).
        auto cornerIndex(key orientation, cube::Corner corner) -> cube::Corner;

    } // namespace orient

} // namespace eltanin::mech::space
