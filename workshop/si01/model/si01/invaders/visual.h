#pragma once

#include <cmath>

#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <si01/invaders/gameObject.h>
#include <si01/invaders/session.h>

#include <fQSM/api/interface.h>

namespace si01::invaders {

    using namespace fqsm::api;

    inline auto spawnSprite(
        Writing context,
        const Session::Quantum& session,
        index2 pos,
        integer sprite_index,
        float sprite_scale,
        float sprite_bank = 0.0f,
        integer zet = 0,
        rmmr::RGB tint = rmmr::RGB{0.0f, 0.0f, 0.0f}) -> rmmr::scene::actor::Sprite::Id
    {
        return with<rmmr::scene::Flat2d>::createSpriteActor(
            context,
            session.scene,
            rmmr::Pose::from(
                rmmr::Pos{
                    static_cast<float>(pos.x),
                    static_cast<float>(pos.y),
                    static_cast<float>(zet),
                },
                rmmr::HPB{0.0f, 0.0f, sprite_bank}),
            item<rmmr::scene::actor::Sprite>{
                .material = session.material,
                .tint = tint,
                .scale = vec3{sprite_scale, sprite_scale, sprite_scale},
                .pack = session.pack,
                .index = sprite_index,
            });
    }

    inline auto createGameObjectWithSprite(
        Writing context,
        const Session::Quantum& session,
        index2 pos,
        integer sprite_index,
        float sprite_scale,
        float sprite_bank = 0.0f,
        integer zet = 0,
        rmmr::RGB tint = rmmr::RGB{0.0f, 0.0f, 0.0f},
        integer hitpoints = 1) -> GameObject::Id
    {
        return with<GameObject>::create(context, GameObject::Quantum{
            .sprite = spawnSprite(context, session, pos, sprite_index, sprite_scale, sprite_bank, zet, tint),
            .hitpoints = hitpoints,
        });
    }

    inline void syncGameObjectSprite(Writing context, GameObject::Id body, index2 pos) {
        if (not with<GameObject>::exists(context, body)) {
            return;
        }
        const auto& object = with<GameObject>::get(context, body);
        if (not object.sprite) {
            return;
        }
        if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
            return;
        }
        auto node = with<rmmr::scene::Node>::modify(context, *object.sprite);
        node->pose.position.x = static_cast<float>(pos.x);
        node->pose.position.y = static_cast<float>(pos.y);
    }

    inline void destroyGameObjectSprite(Writing context, GameObject::Id body) {
        if (not with<GameObject>::exists(context, body)) {
            return;
        }
        auto object = with<GameObject>::modify(context, body);
        if (not object->sprite) {
            return;
        }
        if (with<rmmr::scene::Node>::exists(context, *object->sprite)) {
            with<rmmr::scene::Node>::remove(context, *object->sprite);
        }
        object->sprite.reset();
    }

    inline auto aabbOverlap(index2 a, index2 a_half, index2 b, index2 b_half) -> bool {
        return std::abs(a.x - b.x) <= (a_half.x + b_half.x)
            and std::abs(a.y - b.y) <= (a_half.y + b_half.y);
    }

}
