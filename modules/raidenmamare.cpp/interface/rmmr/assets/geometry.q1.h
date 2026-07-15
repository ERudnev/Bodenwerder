#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/assets/semantics/geometry.h>
#include <rmmr/resources_old/geometry.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::asset {

    using namespace fqsm::api;

    struct Geometry : Entity<Geometry> {
        struct Channel {
            using Id = primitive::GeometrySemantics::PersistentId;
            using Type = primitive::GeometrySemantics::Type;
            using Layout = vector<Id>;
        };
        struct Quantum {
            string debugName;
            Channel::Layout layout;
            vector<Pos> positions;
            vector<Pos> normals;
            vector<UV> uv0;
            vector<integer> indices;
        };
        struct Always {
            static auto layoutIds(const vector<string>& names) -> Channel::Layout;
        };
        struct Actions : BaseActions {
            static auto compile(Writing, Id, system::Device::Id) -> resource_old::Geometry::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
