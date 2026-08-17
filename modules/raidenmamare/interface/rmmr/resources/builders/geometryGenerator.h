#pragma once

#include <cstdint>

#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>
#include <rmmr/semantics/geometry.h>

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
        vector<vec4> color0;
        vector<integer> indices;
        vector<std::uint64_t> mix0;
        vector<vec2> heat;
    };

    struct GeometryGenerator final {
        static CpuPresentation triangle();
        static CpuPresentation kube();
        static CpuPresentation bagel();
        static CpuPresentation gridPlane();
        static CpuPresentation unitQuad();
        static CpuPresentation sphere();
        static CpuPresentation diamond();
    };

}
