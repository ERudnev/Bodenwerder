#pragma once

#include <Raidenmamare/math.q1.h>
#include <Raidenmamare/primitives/geometrySemantics.h>
#include <Raidenmamare/resources/geometry.q1.h>
#include <Raidenmamare/system/core.q1.h>

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
        };
        struct Always {
            static auto layoutIds(const vector<string>& names) -> Channel::Layout;
        };
        struct Actions : BaseActions {
            static auto compile(Writing, Id, system::Device::Id) -> resource::Geometry::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
