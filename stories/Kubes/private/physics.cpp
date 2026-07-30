#include "physics.h"

#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/root.q1.h>

#include <imgui.h>

namespace kubes::phys {

    namespace {

        constexpr float k_visual_radius = 0.2f;

    } // namespace

    void System::bind(scene::Root::Id scene, resource::geometry::Asset::Id geometry, resource::material::Asset::Id material) {
        bound = Bound{.scene = scene, .geometry = geometry, .material = material};
    }

    void System::drawUi(Writing context) {
        ImGui::Checkbox("Visualise", &state.visualise);
        if (bound) {
            if (state.visualise and not prevState.visualise) {
                createVisuals(context);
            } else if (not state.visualise and prevState.visualise) {
                destroyVisuals(context);
            }
        }
        prevState = state;
    }

    void System::step(Stewarding, int64) {
        // black box: forces → Verlet → constraints (later)
    }

    auto System::addParticle(Writing context, vec3 pos) -> Atom::Id {
        const Atom::Quantum quantum{.current = pos, .prev = pos, .mass = 1.0f};
        const auto atom = with<Atom>::create(context, quantum);
        if (state.visualise and bound) {
            visuals.push_back(spawnVisual(context, atom, quantum));
        }
        return atom;
    }

    auto System::spawnVisual(Writing context, Atom::Id atom_id, const Atom::Quantum& atom) -> Visual::Id {
        const auto& resources = *bound;
        const auto actor = with<scene::Interface>::createSimpleActor(
            context,
            resources.scene,
            Locator{.pos = Pos{atom.current}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::actor::Simple>{
                .geometry = resources.geometry,
                .material = resources.material,
                .albedo = RGB{1.0f, 1.0f, 1.0f},
                .scale = vec3{k_visual_radius, k_visual_radius, k_visual_radius},
            });
        return with<Visual>::create(context, Visual::Quantum{
            .atom = atom_id,
            .actor = actor,
        });
    }

    void System::createVisuals(Writing context) {
        destroyVisuals(context);
        for (const auto [atom_id, atom] : context->aspect<Atom>().items()) {
            visuals.push_back(spawnVisual(context, atom_id, atom));
        }
    }

    void System::destroyVisuals(Writing context) {
        for (const auto id : visuals) {
            if (with<Visual>::exists(context, id)) {
                with<Visual>::remove(context, id);
            }
        }
        visuals.clear();
    }

}
