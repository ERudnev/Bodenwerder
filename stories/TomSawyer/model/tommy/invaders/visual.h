#pragma once

#include <cmath>

#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <tommy/invaders/session.h>

#include <fQSM/api/interface.h>

namespace tommy::invaders {

    using namespace fqsm::api;

    inline auto spawnSprite(
        Writing context,
        const Session::Quantum& session,
        index2 pos,
        integer sprite_index,
        integer zet = 0) -> rmmr::scene::Node::Id
    {
        return with<rmmr::scene::Flat2d>::createSpriteActor(
            context,
            session.scene,
            rmmr::Locator{
                .pos = rmmr::Pos{
                    static_cast<float>(pos.x),
                    static_cast<float>(pos.y),
                    static_cast<float>(zet),
                },
                .euler = rmmr::HPB{0.0f, 0.0f, 0.0f},
            },
            item<rmmr::scene::actor::Sprite>{
                .material = session.material,
                .tint = rmmr::RGB{0.0f, 0.0f, 0.0f},
                .scale = vec3{1.0f, 1.0f, 1.0f},
                .pack = session.pack,
                .index = sprite_index,
            });
    }

    inline void syncVisual(Writing context, rmmr::scene::Node::Id visual, index2 pos) {
        if (not with<rmmr::scene::Node>::exists(context, visual)) {
            return;
        }
        auto node = with<rmmr::scene::Node>::modify(context, visual);
        node->position.x = static_cast<float>(pos.x);
        node->position.y = static_cast<float>(pos.y);
    }

    inline void destroyVisual(Writing context, rmmr::scene::Node::Id visual) {
        if (with<rmmr::scene::Node>::exists(context, visual)) {
            with<rmmr::scene::Node>::remove(context, visual);
        }
    }

    inline auto aabbOverlap(index2 a, index2 a_half, index2 b, index2 b_half) -> bool {
        return std::abs(a.x - b.x) <= (a_half.x + b_half.x)
            and std::abs(a.y - b.y) <= (a_half.y + b_half.y);
    }

}
