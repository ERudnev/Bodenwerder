#pragma once

#include <iQSM/q1/_gateway.h>

namespace Q1CORE::Example::Varph {
    using namespace iqsm::dsl_gateway;

    using eVt = float;
    using distance = float;

    namespace settings {
        inline constexpr distance strong_tolerance = 0.01f;
    }

    struct Spark : Entity<Spark>, Require<> {
        using minkpt = vec4;
        using time = decltype(minkpt{}.w);
        struct Quantum {
            minkpt position;
            eVt locality;
        };
        struct Global {
            time clock;
        };
        static const Invariants invariants;
    };


    struct Inertia : Attribute<Inertia, Spark>, Require<Spark> {
        struct Quantum {
            vec4 prev_pos;
            eVt mass;
        };
        static const Invariants invariants;
    };


    struct Charge : Attribute<Charge, Spark>, Require<Spark> {
        struct Quantum {
            integer value;
        };
        struct Operations {
            static auto value(World, Spark::Id)->integer;
        };
        static const Invariants invariants;
    };


    struct Strong : Attribute<Strong, Spark>, Require<Spark> {
        struct Quantum {
            integer isospin2;
        };
        static const Invariants invariants;
    };


    struct TextDescription : Resource<TextDescription>, Require<> {
        struct Passport {
            string fileName;
        };
        struct Quantum { //@ Q1 "instance" + Passport 
            Passport passport;
            timepoint created;
        };
        static const Invariants invariants;
    };

    struct Electron : Attribute<Electron, Charge>, Require<Charge> {
        struct Quantum {};
        static const Invariants invariants;
    };

    struct Nucleon : Attribute<Nucleon, Strong>, Require<Strong> {
        struct Quantum {
            string legend;
        };
        static const Invariants invariants;
    };

    struct Atom : Entity<Atom>, Require<Nucleon, Electron> {
        struct Quantum {
            std::vector<Nucleon::Id> core;
            std::vector<Electron::Id> captured;
            string legend;
        };
        struct Operations {
            static auto tension(World, Atom::Id)->distance;
        };
        static const Invariants invariants;
    };

    struct Chemical : Component<Chemical, Atom>, Require<Atom> {
        struct Quantum {
            integer ionisation;
            integer valency;
        };
        static const Invariants invariants;
    };

    struct Capture : Entity<Capture>, Require<Atom, Electron> {
        struct Quantum {
            Atom::Id atom;
            Electron::Id electron;
            string legend;
            eVt energy;
        };
        static const Invariants invariants;
    };

    struct Modecule : Entity<Modecule>, Require<Atom, TextDescription> {
        struct Quantum {
            std::vector<Atom::Id> atoms;
            TextDescription::Id descrition;
        };
        static const Invariants invariants;
    };

    struct Binding : Entity<Binding>, Require<Atom> {
        struct Quantum {
            std::vector<Atom::Id> bound;
        };
        static const Invariants invariants;
    };

} // namespace Q1CORE::Example::Varph

