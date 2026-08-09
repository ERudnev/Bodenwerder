#pragma once

#include "shapes.h"

#include <vector>

#include <base/types/common_types.h>
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

    // Discrete cell lattice ({0,1}³ inside a cell; cell indices in the construction grid).
    // Higher-order attachables (plates, devices, …) author here.
    // Project rule: cell geometry is centered on the origin — subtract ½ is fixed, not a parameter.
    namespace cell {
        using index = ivec3;
        // remove?
        using Pose = rmmr::renderer::DiscretePose;

        // Cell-space address of a vertex: which cell, which kube corner (recipe cellVertex).
        // Not unique for a grid node (up to 8 cells share a vertex).
        struct Placement {
            index cell;
            cube::Corner corner; // 0..7 in the cell's kube frame (before cell.ori maps to AABB)
        };

        // Lattice integer coords (typically cube::corners {0,1}³) → meters, cell-centered.
        inline auto cell2local(cell::index lattice) -> local::point { return (local::point(lattice) - local::point{0.5f}) * local::edge2meters; }

        // Inverse of cell2local on the lattice image (round for float noise).
        inline auto local2cell(local::point meters) -> index { return index(glm::round(meters / local::edge2meters + local::point{0.5f})); }

        // World meters of a cell's center when continuous 0 = grid::0 (cell i spans grid [i, i+1]).
        inline auto center2local(index cell) -> local::point { return (local::point(cell) + local::point{0.5f}) * local::edge2meters; }

    } // namespace cell

    // Discrete grid-node lattice (vertices / edges / walls). Ship frame (corners, sticks) authors here.
    // World meters are tied to nodes — no half-cell shift (unlike cell::cell2local).
    namespace grid {
        using point = ivec3;
        using Pose = rmmr::renderer::DiscretePose;

        inline auto grid2local(point node) -> local::point { return local::point(node) * local::edge2meters; }

        inline auto local2grid(local::point meters) -> point { return point(glm::round(meters / local::edge2meters)); }
    }

    // Cube orientation alphabet (RedStar CubicRotation). Key 0..23; 0 = identity.
    // matrix[i] — 3×3 in {-1,0,1}; listed by rows (see space.cpp).
    // turn(Semiaxis) — after ±90° about local axis: new key = turn(axis)[old].
    namespace orient {

        using key = int;

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
