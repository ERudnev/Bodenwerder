#include <rmmr/wrapper/ui.h>

#include <imgui.h>

#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/window.q1.h>

namespace rmmr::wrapper::ui {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        auto firstWindow(Reading world) -> base::maybe<system::Window::Id> {
            for (const auto entry : world->aspect<system::Window>().items())
                return entry.id;
            return {};
        }

    } // namespace

    void State::drawMainMenuBar(Product& product) {
        if (not ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Stats", nullptr, &stats);
            product.contributeViewMenu();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void State::drawStatsWindow(Writing world) {
        if (not stats)
            return;

        bool open = stats;
        if (ImGui::Begin("Stats", &open)) {
            const auto window = firstWindow(world);
            if (window.exists()) {
                const auto dt = with<system::Window>::dt(world, *window);
                const auto fps = dt > 0.0 ? 1.0 / dt : 0.0;
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Frame time: %.3f ms", dt * 1000.0);
            } else {
                ImGui::TextDisabled("No active window.");
            }

            ImGui::Separator();
            ImGui::Text("Materials: %zu", with<resource::material::Asset>::count(world));
            ImGui::Text("Textures: %zu", with<resource::texture::Asset>::count(world));
        }
        ImGui::End();
        stats = open;
    }

    void State::draw(Writing world, Product& product) {
        drawMainMenuBar(product);
        drawStatsWindow(world);
        product.drawUi(world);
    }

}
