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
        float sprite_scale,
        float sprite_bank = 0.0f,
        integer zet = 0) -> rmmr::scene::actor::Sprite::Id
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
                .euler = rmmr::HPB{0.0f, 0.0f, sprite_bank},
            },
            item<rmmr::scene::actor::Sprite>{
                .material = session.material,
                .tint = rmmr::RGB{0.0f, 0.0f, 0.0f},
                .scale = vec3{sprite_scale, sprite_scale, sprite_scale},
                .pack = session.pack,
                .index = sprite_index,
            });
    }

    inline auto createSomethingWithSprite(
        Writing context,
        const Session::Quantum& session,
        index2 pos,
        integer sprite_index,
        float sprite_scale,
        float sprite_bank = 0.0f,
        integer zet = 0) -> Something::Id
    {
        return with<Something>::create(context, Something::Quantum{
            .sprite = spawnSprite(context, session, pos, sprite_index, sprite_scale, sprite_bank, zet),
        });
    }

    inline void syncSomethingSprite(Writing context, Something::Id body, index2 pos) {
        if (not with<Something>::exists(context, body)) {
            return;
        }
        const auto& something = with<Something>::get(context, body);
        if (not something.sprite) {
            return;
        }
        if (not with<rmmr::scene::Node>::exists(context, *something.sprite)) {
            return;
        }
        auto node = with<rmmr::scene::Node>::modify(context, *something.sprite);
        node->position.x = static_cast<float>(pos.x);
        node->position.y = static_cast<float>(pos.y);
    }

    inline void destroySomethingSprite(Writing context, Something::Id body) {
        if (not with<Something>::exists(context, body)) {
            return;
        }
        auto something = with<Something>::modify(context, body);
        if (not something->sprite) {
            return;
        }
        if (with<rmmr::scene::Node>::exists(context, *something->sprite)) {
            with<rmmr::scene::Node>::remove(context, *something->sprite);
        }
        something->sprite.reset();
    }

    inline auto aabbOverlap(index2 a, index2 a_half, index2 b, index2 b_half) -> bool {
        return std::abs(a.x - b.x) <= (a_half.x + b_half.x)
            and std::abs(a.y - b.y) <= (a_half.y + b_half.y);
    }

}
