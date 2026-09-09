#pragma once

#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>
#include <unordered_map>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    // Locality heightfield bag (not a domain entity). Lives on Thing::Global.landscape.
    struct Landscape {
        struct Look {
            integer seed;
            float radius;
            float maxRelief;
            float surfaceAcceleration;
            float ridge;
        };

        struct PatchKey {
            std::uint8_t face;
            std::uint8_t level;
            std::uint16_t iu;
            std::uint16_t iv;

            auto operator==(const PatchKey&) const -> bool = default;
        };

        struct PatchKeyHash {
            auto operator()(const PatchKey& key) const noexcept -> std::size_t {
                return (static_cast<std::size_t>(key.face) << 40) ^ (static_cast<std::size_t>(key.level) << 32) ^ (static_cast<std::size_t>(key.iu) << 16) ^ static_cast<std::size_t>(key.iv);
            }
        };

        struct Patch {
            rmmr::scene::actor::Mesh::Id actor;
            rmmr::resource::geometry::Asset::Id geometry;
            std::uint8_t coarserEdges;
        };

        Look look;
        rmmr::Pose pose;
        rmmr::system::Device::Id device;
        phys::Body::Id well;
        rmmr::resource::material::Asset::Id material;
        rmmr::resource::texpack::Pack::Id crust;
        std::unordered_map<PatchKey, Patch, PatchKeyHash> patches;
    };

    struct Planetoid {
        using Look = Landscape::Look;

        struct Surface {
            float height;
            rmmr::vec3 position;
            rmmr::vec3 normal;
            std::uint64_t mix;
            float slope;
        };

        static auto placed(Reading) -> bool;
        static void place(Writing, rmmr::system::Device::Id, rmmr::Pose, Look);
        static void update(Writing, rmmr::Pos camera);

        static auto height(Reading, rmmr::vec3 dir) -> float;
        static auto altitudeAt(Reading, rmmr::Pos worldPos) -> float;
        static auto gravityAt(Reading, rmmr::Pos worldPos) -> rmmr::vec3;
        static auto surfaceInfo(Reading, rmmr::vec3 dir) -> Surface;
    };

}
