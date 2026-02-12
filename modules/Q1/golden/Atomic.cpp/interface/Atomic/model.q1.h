#pragma once

#include <iQSM/q1/_gateway.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Q1CORE::Example::Model {
    using namespace iqsm::dsl_gateway;

    struct Vec3 {
        float x;
        float y;
        float z;
    };

    struct Element : Xion<Element>, DependsFrom<> {
        struct Quantum {
            string name;
            seconds halflife;
            integer valency;
        };
    };

    struct Molecule : Xion<Molecule>, DependsFrom<> {
        struct Quantum {
            string name;
        };
    };

    struct Atom : Xion<Atom>, DependsFrom<Element, Molecule>{ 
        struct Quantum {
            Element::Id type;
            Molecule::Id molecule; //@anchor
        };

        inline static const Structural invariants{{{
            Structural::anchor<Molecule, Atom, &Quantum::molecule>,
        }}};

        //inline static const iqsm::ops::validators::List validators{{
        //    iqsm::validators::anchor<Molecule, Atom, &Quantum::molecule>,
        //}};
    };

    struct Position : Quark<Position, Atom>, DependsFrom<Atom> {
        struct Quantum {
            Vec3 position;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Position>,
        }}};
    };

    struct Chemistry : Quark<Chemistry, Atom>, DependsFrom<Atom, Position> {
        struct Link {
            Id target;
            float strength;
            float distance;
        };

        struct Quantum {
            std::vector<Link> links;
        };
    };

    struct Kinematics : Quark<Kinematics, Atom>, DependsFrom<Atom, Position> {
        struct Quantum {
            float mass;
            Vec3 prev_pos;
        };
    };

    struct Actor : Quark<Actor, Atom>, DependsFrom<Atom, Position> {
        enum class Color { red, green, blue };

        struct Quantum {
            Color color;
        };
    };

    struct Marked : Quark<Marked, Atom>, DependsFrom<Atom> {
        struct Quantum {};
    };

} // namespace Q1CORE::Example::Model