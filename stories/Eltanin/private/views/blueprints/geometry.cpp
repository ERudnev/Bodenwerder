#include "views/blueprints/geometry.h"

#include "mech/semantics/shapes.h"
#include "mech/semantics/subframe.h"

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <cstddef>

#include <base/logging.h>

#include <format>
#include <string>

#include <glm/gtc/quaternion.hpp>

namespace eltanin::views::blueprints::geometry {

    using namespace rmmr;
    using namespace rmmr::resource;

    namespace {

        auto cornerMesh(mech::subframe::corner::kind kind) -> std::string {
            return std::string{mech::subframe::corner::specs.at(kind).code};
        }

        auto halfEdgeMesh(mech::subframe::halfEdge::kind kind, mech::subframe::halfEdge::Pole pole) -> std::string {
            const char poleTag = pole == mech::subframe::halfEdge::Pole::s ? 's' : 'e';
            return std::format("{}{}", mech::subframe::halfEdge::specs.at(kind).code, poleTag);
        }

        auto destroyActor(Writing context, scene::Root::Id root, scene::actor::Mesh::Id actor) -> void {
            if (with<scene::Node_group>::exists(context, root) and with<scene::Node_group>::get(context, root).contains(actor)) {
                with<scene::Node_group>::deleteElement(context, root, actor);
                return;
            }
            if (with<scene::Node>::exists(context, actor))
                with<scene::Node>::remove(context, actor);
        }

        auto entryOrigin(Reading context, const meshpack::Asset::Resolved& resolved) -> base::maybe<Pos> {
            if (not with<::rmmr::resource::geometry::Asset>::exists(context, resolved.geometry))
                return {};
            const auto& asset = with<::rmmr::resource::geometry::Asset>::get(context, resolved.geometry);
            if (resolved.entry >= asset.entries.size())
                return {};
            return asset.entries[resolved.entry].origin;
        }

        auto spawnIdentified(Writing context, scene::Root::Id root, Pose pose, const meshpack::Asset::Resolved& resolved) -> base::maybe<scene::actor::Mesh::Id> {
            const auto id = with<scene::Interface>::createMeshActor(context, root, pose, resolved);
            if (not with<scene::actor::Mesh>::exists(context, id))
                return {};
            with<scene::actor::Identified>::extend(context, id);
            return id;
        }

    } // namespace

    auto localSeatFromOrigin(Pos origin) -> mech::cube::Corner {
        // ±2 m home cube: negative → 0, non-negative → 1
        const auto bit = mech::space::ivec3{
            origin.x >= 0.0f ? 1 : 0,
            origin.y >= 0.0f ? 1 : 0,
            origin.z >= 0.0f ? 1 : 0,
        };
        for (std::size_t i = 0; i < mech::cube::corners.size(); ++i) {
            if (mech::cube::corners[i] == bit)
                return static_cast<mech::cube::Corner>(i);
        }
        return 0;
    }

    auto actorPose(const mech::space::cell::Pose& quarkPose, Pos entryOrigin) -> Pose {
        const auto ori = static_cast<mech::space::orient::key>(quarkPose.ori);
        const auto localSeat = localSeatFromOrigin(entryOrigin);
        const auto seat = mech::space::orient::cornerIndex(ori, localSeat);
        const auto& corner = mech::cube::corners[static_cast<std::size_t>(seat)];
        const float edge = mech::space::local::edge2meters;
        const Pos position{
            (static_cast<float>(quarkPose.pos.x) + static_cast<float>(corner.x)) * edge,
            (static_cast<float>(quarkPose.pos.y) + static_cast<float>(corner.y)) * edge,
            (static_cast<float>(quarkPose.pos.z) + static_cast<float>(corner.z)) * edge,
        };
        const mat3 rotation = mat3(mech::space::orient::matrix[static_cast<std::size_t>(ori)]);
        return Pose{.position = position, .rotation = glm::normalize(glm::quat_cast(rotation))};
    }

    auto resolveKnot(Reading context, meshpack::Asset::Id pack, mech::quarks::Knot::Kind kind) -> base::maybe<meshpack::Asset::Resolved> {
        return with<meshpack::Asset>::resolve(context, pack, cornerMesh(kind));
    }

    auto resolveHalfChord(Reading context, meshpack::Asset::Id pack, mech::quarks::HalfChord::Kind kind, mech::subframe::halfEdge::Pole pole) -> base::maybe<meshpack::Asset::Resolved> {
        return with<meshpack::Asset>::resolve(context, pack, halfEdgeMesh(kind, pole));
    }

    void clearActors(Writing context, scene::Root::Id root, std::vector<QuarkActor>& actors) {
        for (const auto& actor : actors)
            destroyActor(context, root, actor.id);
        actors.clear();
    }

    void syncActors(Writing context, scene::Root::Id root, meshpack::Asset::Id interframe, const mech::Blueprint& blueprint, std::vector<QuarkActor>& actors) {
        clearActors(context, root, actors);

        for (std::size_t i = 0; i < blueprint.knots.size(); ++i) {
            const auto& knot = blueprint.knots[i];
            const auto resolved = resolveKnot(context, interframe, knot.kind);
            if (not resolved) {
                base::message("eltanin blueprints geometry: knot mesh missing for kind");
                continue;
            }
            const auto origin = entryOrigin(context, *resolved);
            if (not origin)
                continue;
            if (const auto id = spawnIdentified(context, root, actorPose(knot.pose, *origin), *resolved))
                actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::knot, .index = i});
        }

        for (std::size_t i = 0; i < blueprint.halfChords.size(); ++i) {
            const auto& halfChord = blueprint.halfChords[i];
            const auto resolved = resolveHalfChord(context, interframe, halfChord.kind, halfChord.pole);
            if (not resolved) {
                base::message("eltanin blueprints geometry: half-chord mesh missing");
                continue;
            }
            const auto origin = entryOrigin(context, *resolved);
            if (not origin)
                continue;
            if (const auto id = spawnIdentified(context, root, actorPose(halfChord.pose, *origin), *resolved))
                actors.push_back(QuarkActor{.id = *id, .kind = QuarkActor::Kind::halfChord, .index = i});
        }
    }

} // namespace eltanin::views::blueprints::geometry
