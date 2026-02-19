#pragma once

#include <iQSM/q1/_gateway.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Q1CORE::Example::Varph {
    using namespace iqsm::dsl_gateway;

    using eVt = float;

    struct Spark : Xion<Spark>, Require<> {
        struct Quantum {
            vec3 position;
            eVt locality;
        };

        inline static const Structural invariants{{{
        }}};
    };

    struct Inertia : Quark<Inertia, Spark>, Require<Spark> {
        struct Quantum {
            vec3 prev_pos;
            eVt mass;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Spark, Inertia>,
        }}};
    };

    struct Electro : Quark<Electro, Spark>, Require<Spark> {
        struct Quantum {
            integer charge;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Spark, Electro>,
        }}};
    };

    struct Atom : Xion<Atom>, Require<Spark> {
        struct Quantum {
            std::vector<Spark::Id> core;
            std::vector<Spark::Id> captured;
            string legend;
        };

        inline static const Structural invariants{{{
            Structural::anchor_any<Spark, Atom, &Quantum::core>,
        }}};
    };

    struct Chemical : Quark<Chemical, Atom>, Require<Atom> {
        struct Quantum {
            integer ionisation;
            integer valency;
        };

        inline static const Structural invariants{{{
            Structural::anchor_quark<Atom, Chemical>,
        }}};
    };

    struct Electron : Xion<Electron>, Require<Spark> {
        struct Quantum {
            Spark::Id spark;
            string legend;
        };

        inline static const Structural invariants{{{
            Structural::anchor<Spark, Electron, &Quantum::spark>,
        }}};
    };

    struct Capture : Xion<Capture>, Require<Atom, Electron> {
        struct Quantum {
            Atom::Id atom;
            Electron::Id electron;
            string legend;
            eVt energy;
        };

        inline static const Structural invariants{{{
            Structural::anchor<Atom, Capture, &Quantum::atom>,
            Structural::anchor<Electron, Capture, &Quantum::electron>,
        }}};
    };

    struct Modecule : Xion<Modecule>, Require<Atom> {
        struct Quantum {
            std::vector<Atom::Id> atoms;
            string legend;
        };

        inline static const Structural invariants{{{
        }}};
    };

    struct Binding : Xion<Binding>, Require<Atom> {
        struct Quantum {
            std::vector<Atom::Id> bound;
        };

        inline static const Structural invariants{{{
            Structural::anchor_all<Atom, Binding, &Quantum::bound>,
        }}};
    };

} // namespace Q1CORE::Example::Varph

