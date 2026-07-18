#pragma once

#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/semantics/geometry.h>

namespace rmmr::resource::builders::geometry {

    using namespace fqsm::api;

    // Intermediate CPU mesh for Loader/Generator internals (not an Asset field).
    struct CpuPresentation {
        struct Channel {
            using Id = primitive::GeometrySemantics::PersistentId;
            using Type = primitive::GeometrySemantics::Type;
            using Layout = vector<Id>;
        };

        Channel::Layout layout;
        vector<Pos> positions;
        vector<Pos> normals;
        vector<UV> uv0;
        vector<integer> indices;
    };

    struct GeometryGenerator final {
        static CpuPresentation triangle();
        static CpuPresentation kube();
        static CpuPresentation bagel();
        static CpuPresentation gridPlane();
    };

}
