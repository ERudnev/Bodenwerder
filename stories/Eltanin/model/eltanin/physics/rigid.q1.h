#pragma once

#include <eltanin/physics/body.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace eltanin::phys::rigid {

    using namespace fqsm::api;

    struct Hull {
        struct Face {
            vector<integer> points;
            vec3 normal;
            float thickness;
        };
        struct Bvh {
            struct Node {
                vec3 boundMin;
                vec3 boundMax;
                integer left;
                integer right;
                integer face;
            };
            vector<Node> nodes;
            integer root;
        };
        vector<Face> faces;
        Bvh bvh;
    };

    struct Ball : Feature<Ball, Body> {
        struct Quantum {
            Particle center;
            quat prevOri;
            vec3 forceAngular;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Crystal : Feature<Crystal, Body> {
        struct Quantum {
            vector<Particle> particles;
            vector<vec3> shape;
            vec3 com;
            Hull hull;

            void refreshMatter(Body::Quantum&);
        };
        struct Actions : BaseActions {
            static void debugAddImpulse(Writing, Id, vec3 imp);
            static void setMotion(Writing, Id, rmmr::Pose, vec3 linear, vec3 omega);
            static void restore(Stewarding);
            static void applyRestored(Stewarding);
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

    auto restoredBody(dvec3 origin, quat rotation, const vector<Particle>&, const vector<vec3>&) -> Body::Quantum;
    auto restoredBody(rmmr::Pose, const vector<Particle>&, const vector<vec3>&) -> Body::Quantum;

}
