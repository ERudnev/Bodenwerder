#pragma once

#include <eltanin/physics/rigid.q1.h>

namespace eltanin::phys::collision {

    using namespace rmmr;

    struct SurfaceHit {
        integer face;
        vec3 localClosest;
        vec3 localOutward;
        float signedDistance;
    };

    void cookHullBvh(rigid::Hull& hull, const vector<vec3>& shape);

    auto closestOnFace(const rigid::Hull& hull, const vector<vec3>& shape, integer faceIndex, vec3 localPoint, vec3& localClosest, vec3& localOutward) -> bool;
    auto closestOnHull(const rigid::Hull& hull, const vector<vec3>& shape, vec3 localPoint) -> SurfaceHit;

}
