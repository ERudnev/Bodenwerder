#include <kubes/physics/atom.q1.h>
#include <kubes/world.q1.h>

#include "physics.h"

#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>

namespace kubes::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    void Visual::Actions::update(Writing context) {
        const auto& world = with<World>::get_global(context);
        const bool have_camera = world.camera and with<scene::Node>::exists(context, *world.camera);
        const quat camera_rotation = have_camera
            ? with<scene::Node>::get(context, *world.camera).rotation
            : quat{1.0f, 0.0f, 0.0f, 0.0f};

        for (const auto [id, visual] : context->aspect<Visual>().items()) {
            if (not my::relation(context, id, &Quantum::atom)
                or not my::ward(context, id, &Quantum::actor)) {
                const auto actor = visual.actor;
                with<Visual>::remove(context, id);
                with<scene::actor::Sprite>::kraken(context, actor); // host Node; custody alone only strips Sprite
                continue;
            }
            const auto& atom = with<Atom>::get(context, visual.atom);
            // Same Id: Sprite ward ⇒ Node exists. Billboard via Node; scale from mass, tint from Visual.
            auto node = with<scene::Node>::modify(context, visual.actor);
            node->position = Pos{atom.current};
            if (have_camera) {
                node->rotation = camera_rotation;
            }
            auto sprite = with<scene::actor::Sprite>::modify(context, visual.actor);
            const float scale = sprite_scale_for_mass(atom.mass);
            sprite->scale = vec3{scale, scale, scale};
            sprite->tint = visual.tint;
        }
    }

    struct Visual::Internals : Visual::DefaultInternals {};

    auto Visual::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Visual, scene::actor::Sprite, &Visual::Quantum::actor>{},
        };
    }

}
