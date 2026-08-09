#include <rmmr/scene/actors/sprite.q1.h>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    void Sprite::Actions::setOpacity(Writing context, Id node, float opacity) {
        with<Sprite>::modify(context, node)->opacity = opacity;
        with<MeshState>::modify(context, node)->opacity = opacity;
    }

}
