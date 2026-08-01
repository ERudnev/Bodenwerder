#include "physics/ui.h"

#include <eltanin/resources/geometry.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <base/logging.h>

#include <imgui.h>

namespace eltanin::phys {

    namespace {

        auto first_root(Reading context) -> base::maybe<rmmr::scene::Root::Id> {
            for (const auto [id, _] : context->aspect<rmmr::scene::Root>().items()) {
                return id;
            }
            return {};
        }

        auto locator_from(const rmmr::Pose& pose) -> rmmr::Locator {
            return rmmr::Locator{.pos = pose.position, .euler = pose.hpb()};
        }

        void destroy_actor(Writing context, rmmr::scene::actor::Simple::Id actor) {
            for (const auto [root, group] : context->aspect<rmmr::scene::Node_group>().items()) {
                if (group.contains(actor)) {
                    with<rmmr::scene::Node_group>::deleteElement(context, root, actor);
                    return;
                }
            }
            if (with<rmmr::scene::Node>::exists(context, actor)) {
                with<rmmr::scene::Node>::remove(context, actor);
            }
        }

    } // namespace

    void Ui::onDrawEnabled(Writing context) {
        base::message("eltanin::phys::Ui: draw shapes enabled");
        state.actors.clear();
        bodies.clear();

        const auto root = first_root(context);
        if (not root) {
            base::message("eltanin::phys::Ui: no scene Root; skip shape actors");
            return;
        }
        if (not shapeMaterial) {
            base::message("eltanin::phys::Ui: shapeMaterial unset; skip shape actors");
            return;
        }

        const auto device = with<World>::get_global(context).window;
        for (const auto [atomic_id, atomic] : context->aspect<Atomic>().items()) {
            const auto& shape = with<resource::atomic::Asset>::get(context, atomic.shape);
            if (device) {
                with<::eltanin::resources::AtomicVisualizer>::materialize(context, shape.visualizer, *device);
            }
            const auto actor = with<rmmr::scene::Interface>::createSimpleActor(context, *root, locator_from(atomic.restored), item<rmmr::scene::actor::Simple>{
                .geometry = shape.visualizer,
                .material = *shapeMaterial,
                .albedo = rmmr::RGB{1.0f, 1.0f, 1.0f},
            });
            state.actors.push_back(actor);
            bodies.push_back(atomic_id);
        }
    }

    void Ui::onDrawDisabled(Writing context) {
        base::message("eltanin::phys::Ui: draw shapes disabled");
        for (const auto actor : state.actors) {
            destroy_actor(context, actor);
        }
        state.actors.clear();
        bodies.clear();
    }

    void Ui::onDraw(Writing context) {
        std::size_t slot = 0;
        while (slot < state.actors.size()) {
            const auto actor = state.actors[slot];
            const auto body = bodies[slot];
            if (not with<Atomic>::exists(context, body) or not with<rmmr::scene::actor::Simple>::exists(context, actor)) {
                if (with<rmmr::scene::actor::Simple>::exists(context, actor)) {
                    destroy_actor(context, actor);
                }
                state.actors.erase(state.actors.begin() + static_cast<std::ptrdiff_t>(slot));
                bodies.erase(bodies.begin() + static_cast<std::ptrdiff_t>(slot));
                continue;
            }
            with<rmmr::scene::Node>::modify(context, actor)->pose = with<Atomic>::get(context, body).restored;
            ++slot;
        }
    }

    void Ui::draw(Writing context, bool& open, System& system) {
        if (open) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(
                ImVec2{viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, viewport->WorkPos.y + 48.0f},
                ImGuiCond_FirstUseEver,
                ImVec2{1.0f, 0.0f});
            ImGui::SetNextWindowSize(ImVec2{280.0f, 0.0f}, ImGuiCond_FirstUseEver);

            if (ImGui::Begin("Physics", &open)) {
                ImGui::TextUnformatted("Time scale");
                constexpr float k_scales[] = {
                    1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f,
                    1.0f,
                    2.0f, 4.0f, 8.0f, 16.0f,
                };
                constexpr const char* k_labels[] = {
                    "1/16", "1/8", "1/4", "1/2",
                    "1",
                    "x2", "x4", "x8", "x16",
                };
                for (int i = 0; i < 9; ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::RadioButton(k_labels[i], system.state.time_scale == k_scales[i])) {
                        system.state.time_scale = k_scales[i];
                    }
                }
            }
            ImGui::End();
        }

        if (open and not prevOpen) {
            onDrawEnabled(context);
        } else if (not open and prevOpen) {
            onDrawDisabled(context);
        }
        prevOpen = open;

        if (open) {
            onDraw(context);
        }
    }

}
