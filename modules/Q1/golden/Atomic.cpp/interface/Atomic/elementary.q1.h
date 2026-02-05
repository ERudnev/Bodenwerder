#pragma once

#include <iQSM/_DSL.include.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Atomic {
    using namespace iqsm::dsl_gateway;

    struct Vec3 {
        float x;
        float y;
        float z;
    };

    struct Element : Xion<Element> {
        struct Item {
            std::string name;
            Seconds halflife;
        };
    };

    struct Body : Xion<Body> {
        struct Item {};
    };

    struct Atom : Xion<Atom> {
        struct Item {
            Element::Id type;
            Body::Id link;
        };
    };

    struct Fusion : Quark<Fusion, Atom> {
        struct Item {
            Seconds life;
        };
    };

    struct Chemistry : Quark<Chemistry, Atom> {
        struct Link {
            Id target;
            float strength;
            float distance;
        };

        struct Item {
            std::vector<Link> links;
        };
    };

    struct Kinematics : Quark<Kinematics, Atom> {
        struct Item {
            float mass;
            Vec3 prev_pos;
        };
    };

    struct Visibility : Quark<Visibility, Atom> {
        enum class Color { red, green, blue };

        struct Item {
            Color color;
        };
    };

    struct Marked : Quark<Marked, Atom> {
        struct Item {};
    };
}