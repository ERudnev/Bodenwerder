#include "views/starMap/view.h"

#include "cluster/measure.h"

#include <eltanin/cluster/orbital.q1.h>
#include <eltanin/cluster/starmap/details.q1.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/system/viewInput.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/window.q1.h>

#include <cmath>
#include <numbers>

#include <glm/geometric.hpp>
#include <imgui.h>

namespace eltanin::views::starmap {

    using namespace fqsm::api;
    using namespace rmmr;

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
        scene = root;
        camera = cam;
        view = rmmr::wrapper::Product::View{.viewport = viewport, .scene = root, .camera = cam};
        panels.systems.emplace();
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

    void View::draw(Writing world) {
        if (not panels.systems)
            return;
        bool open = true;
        ImGui::SetNextWindowSize(ImVec2{280.f, 440.f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Systems", &open)) {
            if (ImGui::BeginTable("axes", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("ly", ImGuiTableColumnFlags_WidthFixed, 72.f);
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
                    const dvec3 delta = axis.pose.position / cluster::measure::ly - focus;
                    ImGui::Text("%.2f", glm::length(delta));
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
        if (not open)
            panels.systems.reset();
    }

}
