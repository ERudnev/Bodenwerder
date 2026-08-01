#include <si02/sun.h>

#include <si02/shot.h>

#include <rmmr/scene/node.q1.h>

#include <cmath>

namespace si02 {

    using namespace fqsm::api;

    void Sun::Actions::attract(Writing context) {
        for (const auto sun_entry : context->aspect<Sun>().items()) {
            const auto sun_id = sun_entry.id;
            if (not with<GameObject>::exists(context, sun_id)) {
                continue;
            }
            const auto& sun_object = with<GameObject>::get(context, sun_id);
            if (not sun_object.sprite) {
                continue;
            }
            if (not with<rmmr::scene::Node>::exists(context, *sun_object.sprite)) {
                continue;
            }
            const auto& sun_node = with<rmmr::scene::Node>::get(context, *sun_object.sprite);

            for (const auto entry : context->aspect<Inertia>().items()) {
                const auto id = entry.id;
                if (id == sun_id) {
                    continue;
                }
                if (with<Shot>::exists(context, id) or with<AnimatedDecay>::exists(context, id)) {
                    continue;
                }
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
                const auto& node = with<rmmr::scene::Node>::get(context, *object.sprite);
                const float dx = sun_node.pose.position.x - node.pose.position.x;
                const float dy = sun_node.pose.position.y - node.pose.position.y;
                const float dist_sq = dx * dx + dy * dy;
                if (dist_sq < 1.0f) {
                    continue;
                }
                const float dist = std::sqrt(dist_sq);
                auto inertia = with<Inertia>::modify(context, id);
                inertia->vel.x += Sun::pull * (dx / dist);
                inertia->vel.y += Sun::pull * (dy / dist);
            }
        }
    }

}
