#include "game.h"

#include <imgui.h>

#include <rmmr/wrapper/ui.h>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        template<typename Panel>
        void togglePanel(const char* label, base::maybe<Panel>& panel) {
            bool open = panel.has_value();
            rmmr::wrapper::ui::viewToggle(label, &open);
            if (open == panel.has_value())
                return;
            if (open)
                panel = Panel{};
            else
                panel.reset();
        }

    }

    void Game::contributeViewMenu(Writing) {
        togglePanel("Blueprints", map.menu.blueprints);
    }

    auto Game::activeOverlay() const -> base::maybe<rmmr::resource::overlay::Asset::Id> {
        if (not map.menu.blueprints.has_value() or not assets.blueprintsEditorEffect)
            return {};
        if (blueprints.state.membranes.enabled or blueprints.state.paletteMode)
            return {};
        return assets.blueprintsEditorEffect;
    }

    auto Game::overlaySelection() const -> std::span<const rmmr::renderer::Integer32> {
        if (not map.menu.blueprints.has_value() or blueprints.state.membranes.enabled or blueprints.state.paletteMode)
            return {};
        return blueprints.state.selection.aliases;
    }

    void Game::drawUi(Writing world) {
        if (map.menu.blueprints.has_value()) {
            bool open = true;
            blueprints.draw(world, open, blueprintPack, mountPack);
            if (not open)
                map.menu.blueprints.reset();
        }
        if (map.view)
            blueprints.bindView(views, map.menu.blueprints.has_value(), *map.view);
    }

}
