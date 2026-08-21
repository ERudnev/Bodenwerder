#pragma once

#include <eltanin/physics/body.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys::rigid {

    using namespace fqsm::api;

    struct Compound {
        struct Hull {
            struct Face {
                vector<integer> points;
                vec3 normal;
            };
            vector<Face> faces;
        };
    };

    struct Crystal : Entity<Crystal> {
        struct Quantum {
            vector<Particle> particles;
            vector<vec3> shape;
            vec3 com;
            Body restored;
            Compound::Hull hull;

            void refreshMatter();
        };
        struct Actions : BaseActions {
            static void debugAddImpulse(Writing, Id, vec3 imp);
            static void setMotion(Writing, Id, rmmr::Pose, vec3 linear, vec3 omega);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Octa : Feature<Octa, Crystal> {
        struct Quantum {};
        struct Actions : BaseActions {
            static void satisfy(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Horned : Feature<Horned, Crystal> {
        struct Quantum {};
        struct Actions : BaseActions {
            static void satisfy(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct CelestialGravity : Attribute<CelestialGravity, Crystal> {
        struct Quantum {
            float averageRadius;
            float surfaceAcceleration;

            auto roundOrbitHelper(float distance) const -> float;
        };
        struct Actions : BaseActions {
            static void apply(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    auto restoredBody(rmmr::Pose, const vector<Particle>&, const vector<vec3>&) -> Body;

}
