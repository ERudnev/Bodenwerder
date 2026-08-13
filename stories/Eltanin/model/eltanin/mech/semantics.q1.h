#pragma once

#include <base/types/common_types.h>
#include <rmmr/renderer/gl.q1.h>

// Hand projection of doctrine/mech/semantics.q1.types (types/enums only).
// Tables and helpers live in private/mech/semantics/.

namespace eltanin::mech {

    enum class Role {
        custom,     // has no defined role
        propulsion, // free back for slots; any ori for maneuver sockets
        power,
        gyros,
        weaponry, // top/front open for hardpoints / point defence
        cargo,    // hangar needs slot+socket for hatch
        logistic, // access / hatches as socket
        emissive,
        control,
        living,
    };

    // Level 0 — how a piece lives on the ship (not form, not function).
    enum class Layer {
        skeleton,
        membranes,
        internals,
        externals,
    };

    namespace space {

        namespace orient {

            // Cube orientation alphabet index 0..23. Same storage as DiscretePose.ori.
            using key = rmmr::renderer::Signed32;

        }

        // Discrete grid transform (not cell/quark seating). Mounts and other non-skeleton attachables.
        struct Transform {
            base::common_types::index3 grid;
            orient::key rotation;
        };

    }

    namespace cube {

        // 0..7 — index into corners[] / unit-cube vertex (RedStar coord_index3).
        using Corner = int;

        enum class Face { Xp, Xn, Yp, Yn, Zp, Zn };

    }

    namespace frame {

        enum class shape {
            k8, // full cube — 8 corners, 6 plates
            k7, // one corner removed — 7 corners, 7 plates
            k6, // edge cut — 6 corners, 5 plates
            k4, // tetrahedral remainder — 4 corners, 4 plates
            // Flat membrane halves (ex-wing, cut along): k{N}f{digits} = N cube corners, flat, perimeter edge codes.
            k4f1111, // ex w1111 half — 4-gon 1-1-1-1
            k3f121,  // ex w121 half — triangle 1-2-1
            k4f2121, // ex w2121 half — 4-gon 2-1-2-1
            k3f222,  // ex w222 half — triangle 2-2-2
        };

    }

    namespace plate {

        enum class shape {
            p1111,
            p121,
            p2121,
            p222A,
            p222V,
        };

    }

    namespace skeleton {

        // Discrete cell lattice placement (cube language). Not a general transform.
        struct Placement {
            base::common_types::index3 cell;
            space::orient::key ori;
        };

        enum class Bend {
            deg45,
            deg71,
            deg90,
            deg125,
        };

        // Mesh at cube corner 0 in local frame; ori places it in the cell.
        struct Corner {
            enum class Kind {
                c124,
                c1364,
                c164,
                c134,
                c135,
                c12,
                c13,
                c15,
                c16,
                c34,
                c35,
            };

            Kind kind;
            space::orient::key ori;
        };

        // Half-stick on an edge. pole selects mesh …s / …e suffix.
        struct Halfrib {
            enum class Kind {
                he1deg90,
                he1deg45,
                he3deg71,
                he3deg90,
                he3deg125,
            };

            enum class Pole {
                starts,
                ends,
            };

            Kind kind;
            Pole pole;
            space::orient::key ori;
        };

        struct Membrane {
            enum class Kind {
                u1111,
                u121,
                u2121,
                u222A,
                u222V,
            };

            Kind kind;
            space::orient::key ori;
        };

    }

}
