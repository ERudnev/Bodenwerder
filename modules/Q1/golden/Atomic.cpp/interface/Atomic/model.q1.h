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
        struct ItemState {
            string name;
            seconds halflife;
            integer valency;
        };
    };

    struct Molecule : Xion<Molecule>, DependsFrom<> {
        struct ItemState {
            string name;
        };
    };

    struct Atom : Xion<Atom>, DependsFrom<Element, Molecule>{ 
        struct ItemState {
            Element::Id type;
            Molecule::Id link; //@anchor
        };
    };

    struct Position : Quark<Position, Atom>, DependsFrom<Atom> {
        struct ItemState {
            Vec3 position;
        };
    };

    struct Fusion : Quark<Fusion, Atom>, DependsFrom<Atom> {
        struct ItemState {
            seconds life;
        };
    };

    struct Chemistry : Quark<Chemistry, Atom>, DependsFrom<Atom, Position> {
        struct Link {
            Id target;
            float strength;
            float distance;
        };

        struct ItemState {
            std::vector<Link> links;
        };
    };

    struct Kinematics : Quark<Kinematics, Atom>, DependsFrom<Atom, Position> {
        struct ItemState {
            float mass;
            Vec3 prev_pos;
        };
    };

    struct Actor : Quark<Actor, Atom>, DependsFrom<Atom, Position> {
        enum class Color { red, green, blue };

        struct ItemState {
            Color color;
        };
    };

    struct Marked : Quark<Marked, Atom>, DependsFrom<Atom> {
        struct ItemState {};
    };

} // namespace Q1CORE::Example::Model