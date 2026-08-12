#include "mech/walls.h"

#include "mech/semantics/space.h"
#include "mech/semantics/subframe.h"

#include <base/maybe.h>

#include <cstddef>

namespace eltanin::mech {

    namespace {

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
        // Use .exists() — maybe<LocalOri> must not use if(ori)/if(not ori): LocalOri is integral, MSVC takes operator T&().
        auto localOriForFace(plate::shape plate, const cube::Loop& targetFace) -> base::maybe<quarks::LocalOri> {
            const auto plateIndex = static_cast<std::size_t>(plate);
            if (plateIndex >= plate::perimeter.size())
                return {};
            const auto& canonical = plate::perimeter[plateIndex];
            for (space::orient::key ori = 0; ori < 24; ++ori) {
                if (cyclicEqualOrReversed(mapLoop(canonical, ori), targetFace))
                    return static_cast<quarks::LocalOri>(ori);
            }
            return {};
        }

        auto wallExists(const Blueprint::Cell& cell, quarks::Wall::Kind kind, quarks::LocalOri ori) -> bool {
            for (const auto& wall : cell.hull.walls) {
                if (wall.kind == kind and wall.ori == ori)
                    return true;
            }
            return false;
        }

    } // namespace

    auto occupiedCorners(const Blueprint::Cell& cell) -> std::vector<bool> {
        std::vector<bool> occupied(8, false);
        for (const auto& knot : cell.frame.knots) {
            const auto corner = space::orient::cornerIndex(static_cast<space::orient::key>(knot.ori), 0);
            if (corner >= 0 and corner < 8)
                occupied[static_cast<std::size_t>(corner)] = true;
        }
        return occupied;
    }

    auto possibleWalls(const Blueprint::Cell& cell) -> std::vector<WallSlot> {
        const auto index = shapeIndex(cell.shape);
        if (index >= frame::faces.size() or index >= skinning::default_plate.size())
            return {};

        const auto occupied = occupiedCorners(cell);
        const auto& faces = frame::faces[index];
        const auto& plates = skinning::default_plate[index];
        std::vector<WallSlot> out;
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

            const auto kind = subframe::membrane::kindOf(plate);
            if (wallExists(cell, kind, *ori))
                continue;

            out.push_back(WallSlot{
                .wall = quarks::Wall{.kind = kind, .ori = *ori},
                .face = static_cast<frame::FaceIndex>(face),
            });
        }
        return out;
    }

    auto faceForWall(const Blueprint::Cell& cell, quarks::Wall wall) -> base::maybe<frame::FaceIndex> {
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
            const auto kind = subframe::membrane::kindOf(plate);
            if (kind == wall.kind and *ori == wall.ori)
                return static_cast<frame::FaceIndex>(face);
        }
        return {};
    }

} // namespace eltanin::mech
