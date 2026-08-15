#include "views/blueprints/mountEditor.h"

#include "mech/semantics/space.h"

#include <base/maybe.h>

#include <algorithm>
#include <format>
#include <tuple>
#include <utility>

#include <imgui.h>

namespace eltanin::views::blueprints::mountEditor {

    namespace {

        auto indexLess(const base::common_types::index3& a, const base::common_types::index3& b) -> bool {
            return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
        }

        auto indexEqual(const base::common_types::index3& a, const base::common_types::index3& b) -> bool {
            return a.x == b.x and a.y == b.y and a.z == b.z;
        }

        auto sortedCopy(std::vector<base::common_types::index3> points) -> std::vector<base::common_types::index3> {
            std::sort(points.begin(), points.end(), indexLess);
            return points;
        }

        auto samePointSet(std::vector<base::common_types::index3> a, std::vector<base::common_types::index3> b) -> bool {
            if (a.size() != b.size())
                return false;
            a = sortedCopy(std::move(a));
            b = sortedCopy(std::move(b));
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (not indexEqual(a[i], b[i]))
                    return false;
            }
            return true;
        }

        // Lattice rotation about doubled center d=min+max: p' = R·p + (d−R·d)/2. Also returns that additive shift.
        auto rotateAboutDoubledCenter(const base::common_types::index3& point, mech::space::orient::key rotation, const base::common_types::ivec3& doubledCenter) -> base::maybe<std::pair<base::common_types::index3, base::common_types::ivec3>> {
            const auto& matrix = mech::space::orient::matrix[static_cast<std::size_t>(rotation)];
            const auto rotatedPoint = matrix * mech::space::ivec3{point.x, point.y, point.z};
            const auto rotatedCenter = matrix * doubledCenter;
            const auto delta = doubledCenter - rotatedCenter;
            if ((delta.x & 1) != 0 or (delta.y & 1) != 0 or (delta.z & 1) != 0)
                return {};
            const base::common_types::ivec3 shift{delta.x / 2, delta.y / 2, delta.z / 2};
            return std::pair{
                base::common_types::index3{.x = rotatedPoint.x + shift.x, .y = rotatedPoint.y + shift.y, .z = rotatedPoint.z + shift.z},
                shift,
            };
        }

        auto lexMin(const std::vector<base::common_types::index3>& points) -> base::common_types::index3 {
            auto origin = points.front();
            for (const auto& point : points) {
                if (indexLess(point, origin))
                    origin = point;
            }
            return origin;
        }

        auto rotateLocal(const std::vector<base::common_types::index3>& points, mech::space::orient::key rotation) -> std::vector<base::common_types::index3> {
            const auto& matrix = mech::space::orient::matrix[static_cast<std::size_t>(rotation)];
            std::vector<base::common_types::index3> out;
            out.reserve(points.size());
            for (const auto& point : points) {
                const auto rotated = matrix * mech::space::ivec3{point.x, point.y, point.z};
                out.push_back(base::common_types::index3{.x = rotated.x, .y = rotated.y, .z = rotated.z});
            }
            return out;
        }

        auto orderedShape(const std::vector<base::common_types::index3>& points) -> Ordered {
            if (points.empty())
                return {};
            const auto origin = lexMin(points);
            Ordered rel;
            rel.reserve(points.size());
            for (const auto& point : points)
                rel.push_back(base::common_types::index3{.x = point.x - origin.x, .y = point.y - origin.y, .z = point.z - origin.z});
            std::sort(rel.begin(), rel.end(), indexLess);
            return rel;
        }

        // Any lattice normal of a flat point set (sign arbitrary). Empty if not a unique plane.
        auto planeNormal(const std::vector<base::common_types::index3>& points) -> base::maybe<mech::space::ivec3> {
            if (points.size() < 3)
                return {};
            const auto& origin = points.front();
            mech::space::ivec3 edge1{};
            bool haveEdge = false;
            for (std::size_t i = 1; i < points.size(); ++i) {
                edge1 = mech::space::ivec3{points[i].x - origin.x, points[i].y - origin.y, points[i].z - origin.z};
                if (edge1.x != 0 or edge1.y != 0 or edge1.z != 0) {
                    haveEdge = true;
                    break;
                }
            }
            if (not haveEdge)
                return {};
            for (std::size_t i = 1; i < points.size(); ++i) {
                const mech::space::ivec3 edge2{points[i].x - origin.x, points[i].y - origin.y, points[i].z - origin.z};
                const mech::space::ivec3 normal{
                    edge1.y * edge2.z - edge1.z * edge2.y,
                    edge1.z * edge2.x - edge1.x * edge2.z,
                    edge1.x * edge2.y - edge1.y * edge2.x,
                };
                if (normal.x == 0 and normal.y == 0 and normal.z == 0)
                    continue;
                for (const auto& point : points) {
                    const mech::space::ivec3 delta{point.x - origin.x, point.y - origin.y, point.z - origin.z};
                    if (delta.x * normal.x + delta.y * normal.y + delta.z * normal.z != 0)
                        return {};
                }
                return normal;
            }
            return {};
        }

        auto mapDir(mech::space::orient::key rotation, mech::space::ivec3 dir) -> mech::space::ivec3 {
            return mech::space::orient::matrix[static_cast<std::size_t>(rotation)] * dir;
        }

        auto sameDir(mech::space::ivec3 a, mech::space::ivec3 b) -> bool {
            return a.x == b.x and a.y == b.y and a.z == b.z;
        }

    } // namespace

    auto OrderedLess::operator()(const Ordered& a, const Ordered& b) const -> bool {
        if (a.size() != b.size())
            return a.size() < b.size();
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (indexLess(a[i], b[i]))
                return true;
            if (indexLess(b[i], a[i]))
                return false;
        }
        return false;
    }

    auto buildSpins(const mech::Attachment& attachment) -> Spins {
        Spins out;
        if (attachment.points.empty())
            return out;

        auto min = attachment.points.front();
        auto max = min;
        for (const auto& point : attachment.points) {
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            min.z = std::min(min.z, point.z);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
            max.z = std::max(max.z, point.z);
        }
        const base::common_types::ivec3 doubledCenter{min.x + max.x, min.y + max.y, min.z + max.z};
        const auto normal = planeNormal(attachment.points);
        base::maybe<std::pair<mech::space::orient::key, Spin>> flipKeep;

        for (mech::space::orient::key rotation = 0; rotation < 24; ++rotation) {
            std::vector<base::common_types::index3> rotated;
            rotated.reserve(attachment.points.size());
            base::common_types::ivec3 shift{};
            bool ok = true;
            bool haveShift = false;
            for (const auto& point : attachment.points) {
                const auto image = rotateAboutDoubledCenter(point, rotation, doubledCenter);
                if (not image) {
                    ok = false;
                    break;
                }
                if (not haveShift) {
                    shift = image->second;
                    haveShift = true;
                }
                rotated.push_back(image->first);
            }
            if (not ok)
                continue;
            if (not samePointSet(attachment.points, rotated))
                continue;
            if (normal and not sameDir(mapDir(rotation, *normal), *normal)) {
                if (not flipKeep)
                    flipKeep = std::pair{rotation, Spin{.shift = shift, .points = std::move(rotated), .flip = true}};
                continue;
            }
            out.emplace(rotation, Spin{.shift = shift, .points = std::move(rotated), .flip = false});
        }
        if (flipKeep)
            out.emplace(flipKeep->first, std::move(flipKeep->second));
        return out;
    }

    auto buildFits(const mech::Attachment& attachment) -> Fits {
        Fits out;
        if (attachment.points.empty())
            return out;
        for (mech::space::orient::key rotation = 0; rotation < 24; ++rotation)
            out.try_emplace(orderedShape(rotateLocal(attachment.points, rotation)), rotation);
        return out;
    }

    auto matchesCursor(const Fits& fits, const std::vector<base::common_types::index3>& cursor) -> bool {
        if (fits.empty() or cursor.empty())
            return false;
        return fits.contains(orderedShape(cursor));
    }

    auto seatingOn(const mech::Attachment& attachment, const Fits& fits, const std::vector<base::common_types::index3>& cursor) -> base::maybe<mech::space::Transform> {
        if (attachment.points.empty() or cursor.empty())
            return {};
        const auto found = fits.find(orderedShape(cursor));
        if (found == fits.end())
            return {};
        const auto rotated = rotateLocal(attachment.points, found->second);
        const auto localOrigin = lexMin(rotated);
        const auto cursorOrigin = lexMin(cursor);
        return mech::space::Transform{
            .grid = base::common_types::index3{.x = cursorOrigin.x - localOrigin.x, .y = cursorOrigin.y - localOrigin.y, .z = cursorOrigin.z - localOrigin.z},
            .rotation = found->second,
        };
    }

    auto worldPoints(const mech::Attachment& attachment, const mech::space::Transform& transform) -> std::vector<base::common_types::index3> {
        const auto& matrix = mech::space::orient::matrix[static_cast<std::size_t>(transform.rotation)];
        std::vector<base::common_types::index3> out;
        out.reserve(attachment.points.size());
        for (const auto& point : attachment.points) {
            const auto rotated = matrix * mech::space::ivec3{point.x, point.y, point.z};
            out.push_back(base::common_types::index3{.x = transform.grid.x + rotated.x, .y = transform.grid.y + rotated.y, .z = transform.grid.z + rotated.z});
        }
        return out;
    }

    auto applyOri(const mech::space::Transform& current, const Spins& spins, mech::space::orient::key bodyAuto) -> base::maybe<mech::space::Transform> {
        const auto found = spins.find(bodyAuto);
        if (found == spins.end())
            return {};
        if (bodyAuto == 0)
            return {};
        const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(current.rotation)];
        const auto nextRotation = composeRow[static_cast<std::size_t>(bodyAuto)];
        const auto worldShift = mech::space::orient::matrix[static_cast<std::size_t>(current.rotation)] * found->second.shift;
        return mech::space::Transform{
            .grid = base::common_types::index3{.x = current.grid.x + worldShift.x, .y = current.grid.y + worldShift.y, .z = current.grid.z + worldShift.z},
            .rotation = nextRotation,
        };
    }

    namespace {
        ImVec2 lastOriMenuPos{0.0f, 0.0f};
        ImVec2 lastOriMenuSize{0.0f, 0.0f};
        bool lastOriMenuShown = false;

        auto clampToViewport(ImVec2 pos, ImVec2 size) -> ImVec2 {
            if (const ImGuiViewport* viewport = ImGui::GetMainViewport()) {
                const float maxX = viewport->WorkPos.x + viewport->WorkSize.x - std::max(size.x, 120.0f);
                const float maxY = viewport->WorkPos.y + viewport->WorkSize.y - std::max(size.y, 80.0f);
                pos.x = std::min(pos.x, maxX);
                pos.y = std::min(pos.y, maxY);
                pos.x = std::max(pos.x, viewport->WorkPos.x);
                pos.y = std::max(pos.y, viewport->WorkPos.y);
            }
            return pos;
        }
    } // namespace

    auto drawOriMenu(mech::space::orient::key currentAbs, const Spins& spins) -> base::maybe<mech::space::orient::key> {
        lastOriMenuShown = false;
        if (spins.empty())
            return {};
        const auto mouse = ImGui::GetMousePos();
        ImGui::SetNextWindowPos(clampToViewport(ImVec2{mouse.x + 18.0f, mouse.y + 18.0f}, ImVec2{160.0f, 120.0f}), ImGuiCond_Appearing, ImVec2{0.0f, 0.0f});
        ImGui::SetNextWindowBgAlpha(0.92f);
        constexpr auto popup = "ori##mountEditor.oriMenu";
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
        if (not ImGui::Begin(popup, nullptr, flags)) {
            ImGui::End();
            return {};
        }
        base::maybe<mech::space::orient::key> picked;
        if (spins.size() == 24) {
            ImGui::TextDisabled("full cube · ±90° · ori %d", static_cast<int>(currentAbs));
            ImGui::Separator();
            using Semiaxis = mech::space::orient::Semiaxis;
            struct Option {
                Semiaxis axis;
                const char* label;
            };
            const Option options[] = {
                Option{.axis = Semiaxis::Yn, .label = "turn left"},
                Option{.axis = Semiaxis::Yp, .label = "turn right"},
                Option{.axis = Semiaxis::Zp, .label = "bank up"},
                Option{.axis = Semiaxis::Zn, .label = "bank down"},
                Option{.axis = Semiaxis::Xn, .label = "tilt left"},
                Option{.axis = Semiaxis::Xp, .label = "tilt right"},
            };
            for (const auto& option : options) {
                const auto bodyAuto = mech::space::orient::turn(option.axis)[0];
                if (not spins.contains(bodyAuto))
                    continue;
                ImGui::PushID(static_cast<int>(option.axis));
                if (ImGui::Selectable(option.label))
                    picked = bodyAuto;
                ImGui::PopID();
            }
        } else {
            ImGui::TextDisabled("%zu body spins · from ori %d", spins.size(), static_cast<int>(currentAbs));
            ImGui::Separator();
            const auto& composeRow = mech::space::orient::compose[static_cast<std::size_t>(currentAbs)];
            struct Row {
                mech::space::orient::key bodyAuto;
                mech::space::orient::key absOri;
                bool flip;
            };
            std::vector<Row> rows;
            rows.reserve(spins.size());
            for (const auto& [bodyAuto, spin] : spins) {
                rows.push_back(Row{
                    .bodyAuto = bodyAuto,
                    .absOri = composeRow[static_cast<std::size_t>(bodyAuto)],
                    .flip = spin.flip,
                });
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
                if (a.flip != b.flip)
                    return not a.flip;
                return a.absOri < b.absOri;
            });
            for (const auto& row : rows) {
                const bool selected = row.bodyAuto == 0;
                ImGui::PushID(static_cast<int>(row.bodyAuto));
                const auto label = row.flip
                    ? std::format("{} flip · abs {} · auto {}", selected ? ">" : " ", static_cast<int>(row.absOri), static_cast<int>(row.bodyAuto))
                    : std::format("{} abs {} · auto {}", selected ? ">" : " ", static_cast<int>(row.absOri), static_cast<int>(row.bodyAuto));
                if (ImGui::Selectable(label.c_str(), selected) and not selected)
                    picked = row.bodyAuto;
                ImGui::PopID();
            }
        }
        lastOriMenuPos = ImGui::GetWindowPos();
        lastOriMenuSize = ImGui::GetWindowSize();
        lastOriMenuShown = true;
        ImGui::End();
        return picked;
    }

    auto drawReplaceMenu(const rmmr::resource::Unit::Name& installed, const ReplaceOption& original, const std::vector<ReplaceOption>& alternatives) -> base::maybe<rmmr::resource::Unit::Name> {
        const auto mouse = ImGui::GetMousePos();
        // Glue to ori's right edge each frame (Appearing+clamp parked it off-screen / elsewhere).
        ImVec2 pos = lastOriMenuShown
            ? ImVec2{lastOriMenuPos.x + lastOriMenuSize.x + 8.0f, lastOriMenuPos.y}
            : ImVec2{mouse.x + 18.0f + 168.0f, mouse.y + 18.0f};
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{0.0f, 0.0f});
        ImGui::SetNextWindowBgAlpha(0.92f);
        constexpr auto popup = "replace##mountEditor.replaceMenu";
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
        if (not ImGui::Begin(popup, nullptr, flags)) {
            ImGui::End();
            return {};
        }
        ImGui::TextDisabled("replace · %zu compatible", alternatives.size());
        ImGui::Separator();
        base::maybe<rmmr::resource::Unit::Name> picked;
        const bool originalInstalled = installed == original.name;
        const auto originalText = original.label.empty() ? original.name.own : original.label;
        ImGui::PushID("original");
        if (ImGui::Selectable(std::format("{} {}", originalInstalled ? ">" : " ", originalText).c_str(), originalInstalled) and not originalInstalled)
            picked = original.name;
        ImGui::PopID();
        if (not alternatives.empty()) {
            ImGui::Separator();
            for (std::size_t i = 0; i < alternatives.size(); ++i) {
                const auto& option = alternatives[i];
                const bool selected = installed == option.name;
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(std::format("{} {}", selected ? ">" : " ", option.label).c_str(), selected) and not selected)
                    picked = option.name;
                ImGui::PopID();
            }
        } else {
            ImGui::TextDisabled("(no other footprint matches)");
        }
        ImGui::End();
        return picked;
    }

} // namespace eltanin::views::blueprints::mountEditor
