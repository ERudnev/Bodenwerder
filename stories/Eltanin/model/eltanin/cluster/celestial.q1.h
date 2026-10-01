#pragma once

#include <eltanin/cluster/chemistry/volatiles.q1.h>
#include <eltanin/cluster/orbital.q1.h>
#include <eltanin/cluster/space.q1.h>
#include <eltanin/types.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::cluster {

    using namespace fqsm::api;

    struct Celestial : Feature<Celestial, Axis> {
        using Mass = double;
        struct Quantum {
            space::Position position;
            Mass mass;
            float radius;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Star : Feature<Star, Celestial> {
        struct Quantum {
            Kelvins temperature;
            chemistry::Volatile::Mix mix;
            Myr age;

            auto look() const -> vec3;
            auto spectralClass() const -> string;
            auto kind() const -> string;
            auto metallicity() const -> string;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto celestial() -> Schema;
    }

}
