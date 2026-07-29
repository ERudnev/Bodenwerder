#include <tommy/gameObject.h>

#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>
#include <rmmr/scene/node.q1.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tommy {

    using namespace fqsm::api;

    namespace {

        // Sprite world size: atlas texels × Sprite.scale (see sprite.vert.glsl).
        // Circle diameter = longer atlas side × uniform scale → radius matches drawn sprite.
        auto collider_radius(Reading context, rmmr::scene::actor::Sprite::Id sprite_id) -> float {
            if (not with<rmmr::scene::actor::Sprite>::exists(context, sprite_id)) {
                return 0.0f;
            }
            const auto& sprite = with<rmmr::scene::actor::Sprite>::get(context, sprite_id);
            if (not with<rmmr::resource::sprite::Pack>::exists(context, sprite.pack)) {
                return 0.0f;
            }
            const auto& pack = with<rmmr::resource::sprite::Pack>::get(context, sprite.pack);
            if (sprite.index < 0 or static_cast<std::size_t>(sprite.index) >= pack.entries.size()) {
                return 0.0f;
            }
            const auto& entry = pack.entries[static_cast<std::size_t>(sprite.index)];
            const float width = static_cast<float>(entry.max.x - entry.min.x);
            const float height = static_cast<float>(entry.max.y - entry.min.y);
            const float diameter = std::max(width, height) * sprite.scale.x;
            return 0.5f * diameter;
        }

    } // namespace

    auto GameObject::customAspectReactions() -> const Behavior {
        return {};
    }

    void Physical::Actions::resolveCollisions(Writing context) {
        struct Body {
            Id id;
            rmmr::scene::Node::Id node;
            float radius = 0.0f;
            float mass = 1.0f;
        };
        vector<Body> bodies;
        for (const auto entry : context->aspect<Physical>().items()) {
            const auto id = entry.id;
            if (not with<GameObject>::exists(context, id)) {
                continue;
            }
            const auto& object = with<GameObject>::get(context, id);
            if (not object.sprite) {
                continue;
            }
            if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
                continue;
            }
            const auto& physical = with<Physical>::get(context, id);
            bodies.push_back(Body{
                .id = id,
                .node = *object.sprite,
                .radius = collider_radius(context, *object.sprite),
                .mass = physical.mass > 0.0f ? physical.mass : physical.size,
            });
        }
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                auto node_a = with<rmmr::scene::Node>::modify(context, bodies[i].node);
                auto node_b = with<rmmr::scene::Node>::modify(context, bodies[j].node);
                const float dx = node_b->position.x - node_a->position.x;
                const float dy = node_b->position.y - node_a->position.y;
                const float dist_sq = dx * dx + dy * dy;
                const float min_dist = bodies[i].radius + bodies[j].radius;
                if (min_dist <= 0.0f) {
                    continue;
                }
                if (dist_sq >= min_dist * min_dist) {
                    continue;
                }
                const float dist = dist_sq > 0.0f ? std::sqrt(dist_sq) : 0.0f;
                const float nx = dist > 0.0f ? dx / dist : 1.0f;
                const float ny = dist > 0.0f ? dy / dist : 0.0f;
                const float overlap = min_dist - dist;
                const float mass_sum = bodies[i].mass + bodies[j].mass;
                const float share_a = bodies[j].mass / mass_sum;
                const float share_b = bodies[i].mass / mass_sum;
                node_a->position.x -= nx * overlap * share_a;
                node_a->position.y -= ny * overlap * share_a;
                node_b->position.x += nx * overlap * share_b;
                node_b->position.y += ny * overlap * share_b;
            }
        }
    }

    void Inertia::Actions::update(Writing context) {
        // One World.step unit. Source of vel is outside this aspect.
        constexpr float dt = 1.0f;
        for (const auto entry : context->aspect<Inertia>().items()) {
            const auto id = entry.id;
            if (not with<GameObject>::exists(context, id)) {
                continue;
            }
            const auto& object = with<GameObject>::get(context, id);
            if (not object.sprite) {
                continue;
            }
            if (not with<rmmr::scene::Node>::exists(context, *object.sprite)) {
                continue;
            }
            auto inertia = with<Inertia>::modify(context, id);
            auto node = with<rmmr::scene::Node>::modify(context, *object.sprite);
            node->position += inertia->vel * dt;
            inertia->vel *= (1.0f - inertia->saturation);
        }
    }

}
