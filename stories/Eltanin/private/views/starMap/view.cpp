#include "views/starMap/view.h"

#include "cluster/measure.h"

#include <eltanin/cluster/celestial.q1.h>
#include <eltanin/cluster/orbital.q1.h>
#include <eltanin/cluster/starmap/details.q1.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

namespace eltanin::views::starmap {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto zoomLog(float ly, float lo, float hi) -> float {
            const float safe = std::max(ly, lo);
            return std::clamp((std::log(safe) - std::log(lo)) / (std::log(hi) - std::log(lo)), 0.0f, 1.0f);
        }

        auto zoomFromU(float u, float lo, float hi) -> float {
            return std::exp(std::log(lo) + std::clamp(u, 0.0f, 1.0f) * (std::log(hi) - std::log(lo)));
        }

        void applyOrbitDistance(Writing context, scene::Camera::Id camera, float distance) {
            if (not with<controller::CameraOrbit>::exists(context, camera))
                return;
            auto orbit = with<controller::CameraOrbit>::modify(context, camera);
            orbit->distance = std::clamp(distance, orbit->distanceMin, orbit->distanceMax);
            const quat rotation = Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{orbit->hpb.x, orbit->hpb.y, 0.0f}).rotation;
            const vec3 forward = glm::normalize(rotation * vec3{0.0f, 0.0f, -1.0f});
            auto node = with<scene::Node>::modify(context, camera);
            node->pose.rotation = rotation;
            node->pose.position = orbit->pivot - forward * orbit->distance;
        }

        auto captionFor(float cellLy) -> const char* {
            for (const auto& mark : Visuals::latticeScales()) {
                if (std::abs(mark.cellLy - cellLy) <= mark.cellLy * 0.01f)
                    return mark.caption;
            }
            return "—";
        }

        void kvRow(const char* key, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", key);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        }

        void drawAxisCard(Reading world, cluster::Axis::Id id) {
            if (not with<cluster::Axis>::exists(world, id)) {
                ImGui::TextDisabled("Gone");
                return;
            }
            if (not ImGui::BeginTable("axisCard", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
                return;
            ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 88.f);
            ImGui::TableSetupColumn("value");
            if (with<cluster::starmap::Details>::exists(world, id))
                kvRow("Name", with<cluster::starmap::Details>::get(world, id).name);
            else
                kvRow("Name", "—");
            if (with<cluster::Star>::exists(world, id)) {
                const auto& star = with<cluster::Star>::get(world, id);
                kvRow("Class", star.spectralClass());
                kvRow("Kind", star.kind());
                kvRow("Age", Format::age(star.age));
                kvRow("T", std::format("{:.0f} K", star.temperature));
            }
            if (with<cluster::Celestial>::exists(world, id)) {
                const auto& body = with<cluster::Celestial>::get(world, id);
                const auto& axis = with<cluster::Axis>::get(world, id);
                const dvec3 worldPos = axis.pose.position + axis.pose.orientation * body.position;
                kvRow("Mass", std::format("{:.2f} Msun", body.mass / Mass::sun));
                kvRow("Radius", std::format("{:.2f} Rsun", double(body.radius) / Radius::sun));
                if (with<cluster::Star>::exists(world, id)) {
                    const auto& star = with<cluster::Star>::get(world, id);
                    const double radiusSuns = double(body.radius) / Radius::sun;
                    const double tempRatio = double(star.temperature) / double(Temperature::sun);
                    kvRow("L", std::format("{:.3g} Lsun", radiusSuns * radiusSuns * tempRatio * tempRatio * tempRatio * tempRatio));
                }
                kvRow("Position", std::format("{:.2f}, {:.2f}, {:.2f} ly", worldPos.x / eLY, worldPos.y / eLY, worldPos.z / eLY));
            } else {
                const dvec3 bary = with<cluster::Axis>::get(world, id).pose.position;
                kvRow("Position", std::format("{:.2f}, {:.2f}, {:.2f} ly", bary.x / eLY, bary.y / eLY, bary.z / eLY));
            }
            if (with<cluster::Star>::exists(world, id))
                kvRow("Metal", with<cluster::Star>::get(world, id).metallicity());
            ImGui::EndTable();
        }

    }

    void View::open(Writing context, system::Window::Id window) {
        const auto framebuffer = with<system::Window>::framebufferSize(context, window);
        const auto viewport = with<system::Viewport_group>::addElement(context, window, system::Viewport::Quantum{
            .origin = index2{0, 0},
            .size = framebuffer,
            .clear_color = vec4{0.0f, 0.0f, 0.0f, 1.0f},
        });
        const auto root = with<scene::Interface>::createScene(context);
        if (not visuals.place(context, root, window))
            return;
        const Pos pivot{0.0f, 0.0f, 0.0f};
        const Pos eye{0.0f, 140.0f, 220.0f};
        const auto cam = with<scene::Interface>::createCamera(context, root, Pose::from(eye, HPB{0.0f, -32.0f, 0.0f}), 60.0f * std::numbers::pi_v<float> / 180.0f);
        {
            auto quantum = with<scene::Camera>::modify(context, cam);
            quantum->z_near = 0.05f;
            quantum->z_far = 2000.0f;
        }
        const auto mail = with<system::ViewInput>::create(context);
        input = mail;
        with<controller::CameraOrbit>::create(context, cam, mail, pivot, glm::length(eye - pivot));
        {
            auto orbit = with<controller::CameraOrbit>::modify(context, cam);
            orbit->distanceMin = 1.0e-10f;
            orbit->distanceMax = 500.0f;
        }
        scene = root;
        camera = cam;
        view = rmmr::wrapper::Product::View{.viewport = viewport, .scene = root, .camera = cam};
        hovered.reset();
        selected.reset();
        rmb.empty = false;
        rmb.origin = vec2{0.0f, 0.0f};
        panels.systems.emplace();
        panels.selected.emplace();
        visuals.follow(context, cam);
    }

    void View::bind(Writing context, const cluster::Astronomy& astronomy) {
        if (not scene or not visuals.bind(context, *scene, astronomy))
            return;
        follow(context);
    }

    void View::follow(Writing context) {
        if (camera)
            visuals.follow(context, *camera);
    }

    void View::handlePointer(Writing world) {
        if (selected and not with<cluster::Axis>::exists(world, *selected))
            selected.reset();
        hovered.reset();
        if (not camera)
            return;
        if (ImGui::GetIO().WantCaptureMouse) {
            rmb.empty = false;
            return;
        }
        const ImGuiIO& io = ImGui::GetIO();
        const vec2 mouse{io.MousePos.x, io.MousePos.y};
        hovered = visuals.pickStar(world, *camera, mouse, vec2{io.DisplaySize.x, io.DisplaySize.y});
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) and hovered)
            selected = hovered;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            rmb.empty = not hovered;
            rmb.origin = mouse;
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            const vec2 delta = mouse - rmb.origin;
            if (rmb.empty and glm::dot(delta, delta) < 25.0f)
                selected.reset();
            rmb.empty = false;
        }
    }

    void View::drawHover(Writing world) {
        if (not hovered or not with<cluster::Axis>::exists(world, *hovered))
            return;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        ImGui::SetNextWindowPos(ImVec2{mouse.x + 14.f, mouse.y + 18.f});
        ImGui::SetNextWindowBgAlpha(0.86f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##starmapHover", nullptr, flags)) {
            ImGui::SetWindowFontScale(0.85f);
            drawAxisCard(world, *hovered);
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::End();
    }

    void View::drawSelected(Writing world) {
        if (not panels.selected)
            return;
        bool open = true;
        ImGui::SetNextWindowSize(ImVec2{280.f, 260.f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Selected", &open)) {
            if (not selected or not with<cluster::Axis>::exists(world, *selected))
                ImGui::TextDisabled("Nothing selected");
            else
                drawAxisCard(world, *selected);
        }
        ImGui::End();
        if (not open)
            panels.selected.reset();
    }

    void View::draw(Writing world) {
        handlePointer(world);
        drawHover(world);
        drawSelected(world);
        if (panels.systems) {
            bool open = true;
            ImGui::SetNextWindowSize(ImVec2{320.f, 440.f}, ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Systems", &open)) {
                if (ImGui::BeginTable("axes", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, ImGui::GetContentRegionAvail())) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("ly", ImGuiTableColumnFlags_WidthFixed, 72.f);
                    ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthFixed, 88.f);
                    ImGui::TableHeadersRow();
                    const dvec3 focus{visuals.focus};
                    for (const auto [id, axis] : world->aspect<cluster::Axis>().items()) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (with<cluster::starmap::Details>::exists(world, id))
                            ImGui::TextUnformatted(with<cluster::starmap::Details>::get(world, id).name.c_str());
                        else
                            ImGui::TextDisabled("—");
                        ImGui::TableNextColumn();
                        const dvec3 delta = axis.pose.position / eLY - focus;
                        ImGui::Text("%.2f", glm::length(delta));
                        ImGui::TableNextColumn();
                        if (with<cluster::Star>::exists(world, id))
                            ImGui::TextUnformatted(Format::age(with<cluster::Star>::get(world, id).age).c_str());
                        else
                            ImGui::TextDisabled("—");
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
            if (not open)
                panels.systems.reset();
        }
        drawScale(world);
        if (not panels.view)
            return;
        bool open = true;
        ImGui::SetNextWindowSize(ImVec2{220.f, 80.f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("View", &open))
            ImGui::Checkbox("Grid", &visuals.display.grid);
        ImGui::End();
        if (not open)
            panels.view.reset();
    }

    void View::drawScale(Writing world) {
        const auto marks = Visuals::latticeScales();
        if (marks.empty())
            return;
        float zoomLo = Visuals::homeLy(marks.front().cellLy) * 0.25f;
        float zoomHi = 500.0f;
        if (camera and with<controller::CameraOrbit>::exists(world, *camera)) {
            const auto& orbit = with<controller::CameraOrbit>::get(world, *camera);
            zoomLo = std::max(orbit.distanceMin, zoomLo);
            zoomHi = orbit.distanceMax;
        }
        if (not (zoomHi > zoomLo))
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y - 10.0f}, ImGuiCond_Always, ImVec2{0.5f, 1.0f});
        ImGui::SetNextWindowBgAlpha(0.48f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (not ImGui::Begin("##starmapScale", nullptr, flags)) {
            ImGui::End();
            return;
        }
        ImGui::SetWindowFontScale(0.75f);
        constexpr ImVec2 trackSize{960.0f, 52.0f};
        ImGui::InvisibleButton("##scaleTrack", trackSize);
        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();
        const float width = std::max(p1.x - p0.x, 1.0f);
        const float trackY = p0.y + 20.0f;
        const float barY = p1.y - 10.0f;
        auto tickX = [&](float ly) -> float { return p0.x + zoomLog(ly, zoomLo, zoomHi) * width; };

        if (ImGui::IsItemActive() and camera) {
            float next = zoomFromU((ImGui::GetIO().MousePos.x - p0.x) / width, zoomLo, zoomHi);
            if (ImGui::IsItemActivated()) {
                for (const auto& mark : marks) {
                    const float x = tickX(std::clamp(Visuals::homeLy(mark.cellLy), zoomLo, zoomHi));
                    if (std::abs(ImGui::GetIO().MousePos.x - x) <= 10.0f)
                        next = std::clamp(Visuals::homeLy(mark.cellLy), zoomLo, zoomHi);
                }
            }
            applyOrbitDistance(world, *camera, next);
            follow(world);
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 ink = ImGui::GetColorU32(ImVec4{0.86f, 0.88f, 0.92f, 0.90f});
        const ImU32 dim = ImGui::GetColorU32(ImVec4{0.86f, 0.88f, 0.92f, 0.42f});
        draw->AddLine(ImVec2{p0.x, trackY}, ImVec2{p1.x, trackY}, dim, 2.0f);
        const std::size_t last = marks.size() - 1;
        for (std::size_t index = 0; index < marks.size(); ++index) {
            const auto& mark = marks[index];
            const float x = tickX(std::clamp(Visuals::homeLy(mark.cellLy), zoomLo, zoomHi));
            draw->AddLine(ImVec2{x, trackY - 5.0f}, ImVec2{x, trackY + 5.0f}, ink, 1.5f);
            const ImVec2 text = ImGui::CalcTextSize(mark.caption);
            float tx = x - text.x * 0.5f;
            if (index == 0)
                tx = x;
            else if (index == last)
                tx = x - text.x;
            draw->AddText(ImVec2{tx, p0.y}, ink, mark.caption);
        }
        const float needle = tickX(visuals.scaleLy);
        draw->AddTriangleFilled(ImVec2{needle, trackY + 7.0f}, ImVec2{needle - 5.0f, trackY + 15.0f}, ImVec2{needle + 5.0f, trackY + 15.0f}, ink);

        const char* caption = captionFor(visuals.cellLy);
        const ImVec2 captionSize = ImGui::CalcTextSize(caption);
        float barPx = visuals.pixelWorld > 1.0e-12f ? visuals.cellLy / visuals.pixelWorld : 48.0f;
        barPx = std::clamp(barPx, 28.0f, std::max(28.0f, width - 8.0f - captionSize.x));
        const float group = barPx + 8.0f + captionSize.x;
        const float barX = p0.x + (width - group) * 0.5f;
        draw->AddLine(ImVec2{barX, barY}, ImVec2{barX + barPx, barY}, ink, 3.0f);
        draw->AddLine(ImVec2{barX, barY - 4.0f}, ImVec2{barX, barY + 4.0f}, ink, 2.0f);
        draw->AddLine(ImVec2{barX + barPx, barY - 4.0f}, ImVec2{barX + barPx, barY + 4.0f}, ink, 2.0f);
        draw->AddText(ImVec2{barX + barPx + 8.0f, barY - captionSize.y * 0.5f}, ink, caption);

        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }

}
