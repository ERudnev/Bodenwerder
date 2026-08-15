#include "assembler.h"

#include <base/logging.h>
#include <rmmr/resources/manager.q1.h>

#include <format>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    void Assembler::immediateSpawn(Writing context, mech::Blueprint::Id blueprintId, Pose pose) {
        if (not with<mech::Blueprint>::exists(context, blueprintId)) {
            (void)context.refuse(std::format("eltanin::Assembler: blueprint #{} missing", blueprintId.raw()));
            return;
        }
        const auto& data = with<mech::Blueprint>::get(context, blueprintId);
        const auto& unit = with<resource::Unit>::get(context, blueprintId);
        std::size_t corners = 0;
        std::size_t halfribs = 0;
        std::size_t membranes = 0;
        for (const auto& cell : data.cells) {
            corners += cell.corners.size();
            halfribs += cell.halfribs.size();
            membranes += cell.membranes.size();
        }
        const char* label = data.name.empty() ? unit.name.own.c_str() : data.name.c_str();
        const auto hpb = pose.hpb();
        base::message("eltanin::Assembler stub: '{}' · cells={} corners={} halfribs={} membranes={} mounts={} · pos=[{:.2f},{:.2f},{:.2f}] hpb=[{:.1f},{:.1f},{:.1f}]", label, data.cells.size(), corners, halfribs, membranes, data.mounts.size(), pose.position.x, pose.position.y, pose.position.z, hpb.x, hpb.y, hpb.z);
    }

}
