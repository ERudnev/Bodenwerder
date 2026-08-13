#include "views/blueprints/membraneSlots.h"

#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"

#include <base/maybe.h>

#include <cstddef>

namespace eltanin::views::blueprints::membraneSlots {

    namespace {

        using namespace mech;

        auto shapeIndex(frame::shape shape) -> std::size_t {
            return static_cast<std::size_t>(shape);
        }

        auto cyclicEqual(const cube::Loop& a, const cube::Loop& b) -> bool {
            if (a.size() != b.size() or a.empty())
                return false;
            const auto n = a.size();
            for (std::size_t start = 0; start < n; ++start) {
                bool ok = true;
                for (std::size_t i = 0; i < n; ++i) {
                    if (a[(start + i) % n] != b[i]) {
                        ok = false;
                        break;
                    }
                }
                if (ok)
                    return true;
            }
            return false;
        }

        auto cyclicEqualOrReversed(const cube::Loop& a, const cube::Loop& b) -> bool {
            if (cyclicEqual(a, b))
                return true;
            cube::Loop reversed;
            reversed.reserve(b.size());
            for (std::size_t i = 0; i < b.size(); ++i)
                reversed.push_back(b[b.size() - 1 - i]);
            return cyclicEqual(a, reversed);
        }

        auto mapLoop(const cube::Loop& loop, space::orient::key ori) -> cube::Loop {
            cube::Loop out;
            out.reserve(loop.size());
            for (const auto corner : loop)
                out.push_back(space::orient::cornerIndex(ori, corner));
            return out;
        }

        // Membrane authored on plate::perimeter[plate]; find local ori that lands that loop on targetFace.
        // Use .exists() — maybe<space::orient::key> must not use if(ori)/if(not ori): key is integral, MSVC takes operator T&().
        auto localOriForFace(plate::shape plate, const cube::Loop& targetFace) -> base::maybe<space::orient::key> {
            const auto plateIndex = static_cast<std::size_t>(plate);
            if (plateIndex >= plate::perimeter.size())
                return {};
            const auto& canonical = plate::perimeter[plateIndex];
            for (space::orient::key ori = 0; ori < 24; ++ori) {
                if (cyclicEqualOrReversed(mapLoop(canonical, ori), targetFace))
                    return ori;
            }
            return {};
        }

        auto membraneExists(const Cell& cell, skeleton::Membrane::Kind kind, space::orient::key ori) -> bool {
            for (const auto& membrane : cell.membranes) {
                if (membrane.kind == kind and membrane.ori == ori)
                    return true;
            }
            return false;
        }

        auto occupiedCorners(const Cell& cell) -> std::vector<bool> {
            std::vector<bool> occupied(8, false);
            for (const auto& corner : cell.corners) {
                const auto vertex = space::orient::cornerIndex(static_cast<space::orient::key>(corner.ori), 0);
                if (vertex >= 0 and vertex < 8)
                    occupied[static_cast<std::size_t>(vertex)] = true;
            }
            return occupied;
        }

    } // namespace

    auto possible(const Cell& cell) -> std::vector<Slot> {
        const auto index = shapeIndex(cell.shape);
        if (index >= frame::faces.size() or index >= skinning::default_plate.size())
            return {};

        const auto occupied = occupiedCorners(cell);
        const auto& faces = frame::faces[index];
        const auto& plates = skinning::default_plate[index];
        std::vector<Slot> out;
        out.reserve(faces.size());

        for (std::size_t face = 0; face < faces.size() and face < plates.size(); ++face) {
            const auto& loop = faces[face];
            bool full = true;
            for (const auto corner : loop) {
                if (corner < 0 or corner >= 8 or not occupied[static_cast<std::size_t>(corner)]) {
                    full = false;
                    break;
                }
            }
            if (not full)
                continue;

            const auto plate = plates[face];
            const auto ori = localOriForFace(plate, loop);
            if (not ori.exists())
                continue;

            const auto kind = skeleton::membraneKindOf(plate);
            if (membraneExists(cell, kind, *ori))
                continue;

            out.push_back(Slot{
                .membrane = skeleton::Membrane{.kind = kind, .ori = *ori},
                .face = static_cast<frame::FaceIndex>(face),
            });
        }
        return out;
    }

    auto faceFor(const Cell& cell, mech::skeleton::Membrane membrane) -> base::maybe<mech::frame::FaceIndex> {
        const auto index = shapeIndex(cell.shape);
        if (index >= frame::faces.size() or index >= skinning::default_plate.size())
            return {};
        const auto occupied = occupiedCorners(cell);
        const auto& faces = frame::faces[index];
        const auto& plates = skinning::default_plate[index];
        for (std::size_t face = 0; face < faces.size() and face < plates.size(); ++face) {
            const auto& loop = faces[face];
            bool full = true;
            for (const auto corner : loop) {
                if (corner < 0 or corner >= 8 or not occupied[static_cast<std::size_t>(corner)]) {
                    full = false;
                    break;
                }
            }
            if (not full)
                continue;
            const auto plate = plates[face];
            const auto ori = localOriForFace(plate, loop);
            if (not ori.exists())
                continue;
            const auto kind = skeleton::membraneKindOf(plate);
            if (kind == membrane.kind and *ori == membrane.ori)
                return static_cast<frame::FaceIndex>(face);
        }
        return {};
    }

} // namespace eltanin::views::blueprints::membraneSlots
