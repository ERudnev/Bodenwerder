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

    struct Element : Xion<Element>, Require<> {
        struct Quantum {
            string name;
            seconds halflife;
            integer valency;
        };

        inline static const Structural invariants{{{
        }}};
    };

    struct Molecule : Xion<Molecule>, Require<> {
        struct Quantum {
            string name;
        };

        inline static const Structural invariants{{{
        }}};
    };

    struct Atom : Xion<Atom>, Require<Element, Molecule>{ 
        struct Quantum {
            Element::Id type;
            Molecule::Id molecule; //@anchor
        };

        inline static const Structural invariants{{{
            Structural::anchor<Molecule, Atom, &Quantum::molecule>,
        }}};
    };

    struct Position : Quark<Position, Atom>, Require<Atom> {
        struct Quantum {
            Vec3 position;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Position>,
        }}};
    };

    struct Chemistry : Quark<Chemistry, Atom>, Require<Atom, Position> {
        struct Link {
            Id target;
            float strength;
            float distance;
        };

        struct Quantum {
            std::vector<Link> links;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Chemistry>,
        }}};
    };

    struct Kinematics : Quark<Kinematics, Atom>, Require<Atom, Position> {
        struct Quantum {
            float mass;
            Vec3 prev_pos;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Kinematics>,
        }}};
    };

    struct Actor : Quark<Actor, Atom>, Require<Atom, Position> {
        enum class Color { red, green, blue };

        struct Quantum {
            Color color;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Actor>,
        }}};
    };

    struct Marked : Quark<Marked, Atom>, Require<Atom> {
        struct Quantum {};

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Marked>,
        }}};
    };

} // namespace Q1CORE::Example::Model