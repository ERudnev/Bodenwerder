#pragma once

#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/construction.q1.h>
#include <eltanin/locality/construct.q1.h>
#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/resources/meshpack.q1.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Assembler {
        static auto spawn(Writing, rmmr::scene::Root::Id, rmmr::Pose, Blueprint::Id, vec3 velocity) -> locality::Construct::Id;
    };

    auto cookOccurrences(Reading, rmmr::resource::meshpack::Asset::Id, const Construction&, const locality::Construct::ActorFragments&, vector<Construction::Primitive::Id>& visualOf) -> vector<rmmr::scene::actor::Mesh::Occurrence>;

}
